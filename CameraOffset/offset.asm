
; in
extern CameraOffset : xmmword
extern CollisionOffset : xmmword
extern RetractCollisionOffset : xmmword
extern MaxDistanceInterp : dword
extern TargetOffset : xmmword

extern TargetAimAreaMul : dword
extern TargetViewOffsetMul : dword

;out
extern bLastCollisionHit : byte
extern LastCollisionPos : xmmword
extern LastCollisionDistNormalized : dword

extern TargetViewOffset : xmmword
extern TargetViewMaxOffset : dword

.data
	bApplyOffset byte 1

.code
	SetCameraOffset proc
		push rax
		mov al, [bApplyOffset]
		test al, al
		je skip_offset
		addps xmm1, [CameraOffset]

		skip_offset:
		mov [bApplyOffset], 1
		pop rax
		ret
	SetCameraOffset endp

	SetCollisionOffset proc
		addps xmm6, [CollisionOffset]
		ret
	SetCollisionOffset endp

	AdjustCollision proc
		;subps xmm2, [CameraOffset]

		; this only runs when there's collision in current frame
		mov [bLastCollisionHit], 1
		movaps [LastCollisionPos], xmm2
		mov [bApplyOffset], 0

		; somehow the value lived through the collision function in xmm5, may not work forever
		movss [LastCollisionDistNormalized], xmm5

		ret
	AdjustCollision endp

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
		mulss xmm13, [TargetAimAreaMul]
		movss [TargetViewMaxOffset], xmm13
		movaps [TargetViewOffset], xmm6
		ret
	GetTargetViewOffset endp

	TargetViewYOffset proc
		mulss xmm13, [TargetViewOffsetMul]
		ret
	TargetViewYOffset endp
end