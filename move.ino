void stop() {
	//駆動
	Motor.run(false, 0, 0);
	//LCD表示
	LCD.clear(true);
	LCD.write("THE VOLTAGE", 5, 1);
	LCD.write("IS TOO LOW!!!", 4, 2);
}

void wait(data_t *d) {
	//LCD表示
	LCD.run(d->gyro, d->line, Kicker.getCanUse(), Cam.getCanUse(), bool(d->gyro), isFW, Comc.getCanUse(), d->fellow,
		Line.getQTY(), Line.getValue(), Line.getState(),INA219.getValue(), d->goal,
		d->ball, Ball.getQTY(), Ball.getValue(),
		Ball.getValueCatch(), d->catchingBall, Ball.getForward(), d->isBallForward, d->distBall,
		frontPSD.getValue(), d->enemyStandsFront, backPSD.getValue(), d->distGoalPSD);
	//駆動
	Motor.run(false, 0, 0);
	Kicker.check();
}


void correctRot(bool isFW, Angle gyro) {
	cCorrectRot.set_COUNT_UP(!bool(cCorrectRot));
	if(bool(cCorrectRot)) {
		//駆動
		int16_t powerCorrectRot = absAngle(gyro) >= THRE_INCREASE_CCR
			? signum(gyro) * (isFW ? 120 : 60)
			: signum(gyro) * (isFW ? 20 : 20);
		Motor.run(false, powerCorrectRot, 0);
		//correctRot継続
		cCorrectRot.increase(absAngle(gyro) >= THRE_DECREASE_CCR);
	}else {
		//correctRot開始
		cCorrectRot.increase(absAngle(gyro) >= THRE_INCREASE_CCR);
	}
}

void carryBall(bool isFW, line_t line, int16_t rot, cam_t goal, Angle gyro, bool catchingBall, bool enemyStandsFront) {
	willCarryBall = carryingBall;
	if(carryingBall) {
		if(millis() - timeStartCB < 1500) {
			////
			Motor.run(0, rot * 1.5, isFW ? 230 : 200);
		}else {
			//スタック
			Motor.run(false, 0, 0);
		}
		//carry続けるか
		willCarryBall = Ball.compareCatch(THRE_CONTINUE_CARRY);
	}else {
		//carry始めるか
		willCarryBall = catchingBall;
		if(willCarryBall) {
			timeStartCB = millis();
		}
	}
}


void run(data_t *d, bool isFW, Angle dir, int16_t rot) {
	willCarryBall = false;
	if(isFW) {
		//FW
		bool leavingLine = bool(d->line.dirInside) && dir.inside(d->line.dirInside + 90, d->line.dirInside - 90);
		carryBall(isFW, d->line, rot, d->goal, d->gyro, d->catchingBall || d->isBallForward, d->enemyStandsFront);
		if(d->isBallForward) {
			//ボール前方直線上
			Motor.run(dir, rot * 1.5, leavingLine ? 120 : 210);
		}else if(dir.inside(90, 270)) {
			//ボール後方
			Motor.run(dir, min(rot * 1.5, 60), (leavingLine || d->distGoal == CLOSE) ? 100 : 180);
		}else {
			//ボール前方
			Motor.run(dir, min(rot * 1.5, 60), leavingLine ? 100 : 140);
		}
		Kicker.kick(d->catchFreely && d->goal.isWide);
	}else {
		//GK
		d->isBallForward |= d->ball.t.inside(350, 10) && d->distBall == CLOSE;
		bool isOnSide = abs(d->goal.rot) >= 3 || (d->distGoal == TOO_FAR && abs(d->goal.rot) >= 2);
		if(isOnSide) {
			//横行きすぎ
			Motor.run(180 - signum(d->goal.rot) * (d->distGoal == TOO_FAR ? 60 : 100), rot, 120);
		}else if(d->distGoal == TOO_FAR) {
			//ゴールとても遠すぎ・仲間がいてゴール遠すぎ
			Motor.run(180, rot, 160);
		}
		//ボール捕獲
		carryBall(isFW, d->line, rot, d->goal, d->gyro, d->catchingBall, false);
		if(d->isBallForward) {
			//ボール前方
			Motor.run(0, rot, d->fellow.exists ? 40 : 100);
		}else if(d->distBall == CLOSE && abs(d->goal.rot) >= 1 && signum(d->ball.t - 180) == signum(d->goal.rot)) {
			Motor.run(Ball.getDir(d->ball), 0, 40);
		}else if(!bool(dir)
			||	(signum(d->ball.t - 180) == signum(d->goal.rot) && abs(d->goal.rot) >= 2)
			||	d->ball.t.inside(150, 210)
			||	d->ball.t.inside(355, 5)) {
			//ボールない・ボール外側・ボール後ろ(150~210)・ボール前方遠く
			Motor.run(d->distGoal == CLOSE ? 0 : d->distGoal >= FAR ? 180 : Angle(false),
				d->distGoal == PROPER ? signum(rot) * 60 : 0, 80);
		}else if(d->ball.t.inside(340, 20) || signum(d->goal.rot) != signum(dir)) {
			//ボールある程度前方・少し横
			Motor.run(dir, rot, 120);
		}else {
			//ボール斜め前方
			Motor.run(dir, rot, 210);
		}
		Kicker.kick(d->catchFreely && !d->fellow.exists);
	}
	carryingBall = willCarryBall && d->ball.t.inside(340, 20) && d->distBall == CLOSE;
}