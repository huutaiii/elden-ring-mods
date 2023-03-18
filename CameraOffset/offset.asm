
; in
extern CameraOffset : xmmword
extern CollisionOffset : xmmword
extern RetractCollisionOffset : xmmword
extern MaxDistanceInterp : dword
extern TargetOffset : xmmword

extern SpringbackOffset : xmmword

extern TargetAimAreaMul : dword
extern TargetViewOffsetMul : dword

;out
extern LastWorldPos : xmmword
extern bLastCollisionHit : byte
extern LastCollisionPos : xmmword
extern LastCollisionStart : xmmword
extern LastCollisionEnd : xmmword
extern LastCollisionDistNormalized : dword

extern TargetViewOffset : xmmword
extern TargetViewMaxOffset : dword

extern CalcSpringbackOffset : proto

.data
	bApplyOffset byte 1

.code
	SetCameraOffset proc
		push rax
		push rcx
		push rdx
		lea rsp, [rsp-70h]
		movaps [rsp], xmm1
		movaps [rsp+10h], xmm6
		movaps [rsp+20h], xmm7
		movaps [rsp+30h], xmm8
		movaps [rsp+40h], xmm9
		movaps [rsp+50h], xmm10
		movaps [rsp+60h], xmm11
		
		call CalcSpringbackOffset
		
		movaps xmm1, [rsp]
		movaps xmm6, [rsp+10h]
		movaps xmm7, [rsp+20h]
		movaps xmm8, [rsp+30h]
		movaps xmm9, [rsp+40h]
		movaps xmm10, [rsp+50h]
		movaps xmm11, [rsp+60h]
		lea rsp, [rsp+70h]
		pop rdx
		pop rcx
		pop rax

		push rax
		mov al, [bApplyOffset]
		test al, al
		je skip_offset

		addps xmm1, [CameraOffset]
		addps xmm1, [SpringbackOffset]

		skip_offset:
		mov [bApplyOffset], 1
		pop rax

		movaps [LastWorldPos], xmm1

		ret
	SetCameraOffset endp

	SetCollisionOffset proc
		addps xmm6, [CollisionOffset]

		xorps xmm1, xmm1
		movaps xmm1, [rbp-20h]
		movaps [LastCollisionStart], xmm1
		addps xmm1, xmm6
		movaps [LastCollisionEnd], xmm6

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