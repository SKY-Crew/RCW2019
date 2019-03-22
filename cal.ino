void setThres(bool isFW, bool fellowExists) {
	THRE_IF = isFW ? 610 : fellowExists ? 670 : 610;
	THRE_DB[0] = isFW ? 300 : fellowExists ? 400 : 350;
	THRE_DB[1] = 200;
	THRE_INCREASE_CCR = isFW ? 60 : 30;
	THRE_DECREASE_CCR = isFW ? 40 : 5;
	cCatchFreely.set_MAX(isFW ? 3 : 1);
}

void get(data_t *d) {
	d->gyro = Gyro.get();
	d->goal = Cam.get();
	backPSD.get();
	uint16_t valueBackPSD = backPSD.getValue();
	d->distGoalPSD = valueBackPSD <= THRE_BACK_PSD[0] ? CLOSE : valueBackPSD <= THRE_BACK_PSD[1] ? PROPER : FAR;
	d->distGoal = abs(d->goal.rot) >= 2 || d->goal.distGK == TOO_FAR ? d->goal.distGK : d->distGoalPSD;

	cEnemyStandsFront.increase(frontPSD.get());
	d->enemyStandsFront = bool(cEnemyStandsFront);
	d->fellow = Comc.communicate(canRun, isFW);

	d->ball = Ball.get(false);
	d->distBall = d->ball.r >= THRE_DB[0] ? CLOSE : d->ball.r >= THRE_DB[1] ? PROPER : FAR;
	d->isBallForward = Ball.getForward() >= THRE_IF && d->distBall == CLOSE;
	d->catchingBall = Ball.getCatch() && d->ball.t.inside(330, 30) && d->distBall == CLOSE;
	cCatchFreely.increase(d->catchingBall && !d->enemyStandsFront);
	d->catchFreely = bool(cCatchFreely) && (isFW || d->distGoal == TOO_FAR || !Cam.getCanUse());

	d->line = Line.get(isFW, d->gyro, Gyro.getDiff());
}


Angle calDir(bool isFW, vectorRT_t ball, Angle gyro, cam_t goal, bool distGoal, Dist distBall) {
	Angle dir;
	if(isFW) {
		dir = distBall == FAR ? ball.t : Ball.getDir(ball);
	}else {
		Angle dirGK = distGoal == CLOSE ? 110 : distGoal == PROPER ? 70 : 90;////
		dir = bool(ball.t) ? dirGK * signum(ball.t) : Angle(false);
	}
	return dir;
}

int16_t calRot(bool isFW, cam_t goal, Angle gyro, bool catchingBall, bool isBallForward) {
	//rot計算
	int16_t rot = 0;
	if(isFW) {
		if(Cam.getCanUse() && bool(gyro)) {
			//両方使用可
			rot = (catchingBall || isBallForward) && abs(goal.rotOpp) <= 3
				? Cam.multiRotGoal(goal.rotOpp)
				: Gyro.multiRot(gyro);
		}else if(Cam.getCanUse()) {
			//camのみ
			rot = Cam.multiRotGoal(goal.rotOpp);
		}else if(bool(gyro)) {
			//gyroのみ
			rot = Gyro.multiRot(gyro);
		}
	}else {
		rot = Gyro.multiRot(gyro);
	}
	return rot;
}


void checkRole(bool canBecomeGK, comc_t fellow) {
	if(Comc.getCanUse()) {
		if(fellow.exists && isFW == fellow.isFW) {
			if(canBecomeGK && isFW) {
				//fellowがGK→FW
				isFW = false;
				cCatchFreely.reset();
			}else if(!isFW && !canRun) {
				//停止状態
				isFW = true;
			}else if(!isFW && IS_SKY) {
				//両方GK
				isFW = true;
			}
		}
		if(canRun && !fellow.exists && isFW && canBecomeGK) {
			//fellowいなくなる
			isFW = false;
			cCatchFreely.reset();
		}
	}
	//閾値変更
	setThres(isFW, fellow.exists);
}

bool avoidMulDef(Angle *dir, comc_t fellow, vectorRT_t ball, cam_t goal) {
	bool isGoalClose = false;
	if(fellow.exists) {
		if(ball.t.inside(90, 270)) {
			switch (goal.distFW) {
			//少し後ろ
			case 1: *dir = ball.t.inside(170, 190) ? Angle(false)
						: ball.t.inside(90, 180) ? 90 : 270;
					isGoalClose = true;
					break;
			//後ろ過ぎ
			case 0: *dir = ball.t.inside(170, 190) ? 0
						: ball.t.inside(90, 180) ? 50 : 310;
					isGoalClose = false;
					break;
			}
		}
	}
	return isGoalClose;
}

void detectBallOutside(Angle *dir, line_t line, Angle gyro) {
	if(bool(line.dirInside)) {
		Angle absoluteDI = line.dirInside - gyro;
		if(absoluteDI.inside(45, 135)) {
			if(dir->inside(absoluteDI + 170, absoluteDI + 200)
				&& line.canPause) {
				//停止
				*dir = Angle(false);
			}else if(dir->inside(absoluteDI + 90, absoluteDI + 180)) {
				//後退
				*dir = absoluteDI + 90;
			}
		}else if(absoluteDI.inside(225, 315)) {
			if(dir->inside(absoluteDI + 160, absoluteDI + 190)
				&& line.canPause) {
				//停止
				*dir = Angle(false);
			}else if(dir->inside(absoluteDI - 180, absoluteDI - 90)) {
				//後退
				*dir = absoluteDI - 90;
			}
		}
	}
}