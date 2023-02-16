
; in
extern CameraOffset : xmmword
extern CollisionOffset : xmmword
extern RetractCollisionOffset : xmmword
extern MaxDistanceInterp : dword
extern TargetOffset : xmmword

;out
extern bLastCollisionHit : byte
extern LastCollisionPos : xmmword
extern LastCollisionDistNormalized : dword

extern TargetViewOffset : xmmword
extern TargetViewMaxOffset : dword
extern TargetViewMaxOffsetMul : dword

.data
	iCollision byte 0

.code
	SetCameraOffset proc
		addps xmm1, [CameraOffset]
		ret
	SetCameraOffset endp

	SetCollisionOffset proc
		addps xmm0, [CollisionOffset]
		ret
	SetCollisionOffset endp

	SetCollisionOffsetAlt proc
		;addps xmm0, [CollisionOffset]
		xorps xmm0, xmm0
		ret
	SetCollisionOffsetAlt endp

	SetCollisionOffsetAlt1 proc
		addps xmm6, [RetractCollisionOffset]
		ret
	SetCollisionOffsetAlt1 endp

	AdjustCollision1 proc
		subps xmm2, [CameraOffset]

		; this only runs when there's collision in current frame
		mov [bLastCollisionHit], 1
		movaps [LastCollisionPos], xmm2

		; somehow the value lived through the collision function in xmm5, may not work forever
		movss [LastCollisionDistNormalized], xmm5

		ret
	AdjustCollision1 endp

	AdjustCollision proc
		subps xmm2, [CollisionOffset]
		ret
	AdjustCollision endp

	AdjustCollision01 proc
		;subps xmm3, [CollisionOffset]
		ret
	AdjustCollision01 endp

	ClampMaxDistance proc
		movss xmm0, [MaxDistanceInterp]
		ret
	ClampMaxDistance endp

	SetTargetOffset proc
		subps xmm9, [CameraOffset]
		addps xmm9, [TargetOffset]
		ret
	SetTargetOffset endp

	GetTargetViewOffset proc
		mulss xmm13, [TargetViewMaxOffsetMul]
		movss [TargetViewMaxOffset], xmm13
		movaps [TargetViewOffset], xmm6
		ret
	GetTargetViewOffset endp
end