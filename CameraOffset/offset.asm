
; in
extern CameraOffset : xmmword
extern CollisionOffset : xmmword
extern TargetOffset : xmmword

extern SpringbackOffset : xmmword

extern TargetAimAreaMul : dword
extern TargetViewOffsetMul : dword

;out
extern bLastCollisionHit : byte
extern LastCollisionPos : xmmword
extern LastCollisionEnd : xmmword

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
		push r8
		push r9
		push r10
		push r11
		lea rsp, [rsp-60h]
		movaps [rsp], xmm0
		movaps [rsp+10h], xmm1
		movaps [rsp+20h], xmm2
		movaps [rsp+30h], xmm3
		movaps [rsp+40h], xmm4
		movaps [rsp+50h], xmm5
		
		call CalcSpringbackOffset
		
		movaps xmm0, [rsp]
		movaps xmm1, [rsp+10h]
		movaps xmm2, [rsp+20h]
		movaps xmm3, [rsp+30h]
		movaps xmm4, [rsp+40h]
		movaps xmm5, [rsp+50h]
		lea rsp, [rsp+60h]
		pop r11
		pop r10
		pop r9
		pop r8
		pop rdx
		pop rcx
		pop rax

		push rax
		mov al, [bLastCollisionHit]
		test al, al
		jne skip_offset

		addps xmm1, [CameraOffset]
		addps xmm1, [SpringbackOffset]

		skip_offset:
		pop rax

		ret
	SetCameraOffset endp

	SetCollisionOffset proc
		addps xmm6, [CollisionOffset]
		movaps [LastCollisionEnd], xmm6
		mov [bLastCollisionHit], 0

		ret
	SetCollisionOffset endp

	AdjustCollision proc
		;subps xmm2, [CameraOffset]

		; this only runs when there's collision in current frame
		mov [bLastCollisionHit], 1
		movaps [LastCollisionPos], xmm2

		ret
	AdjustCollision endp

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