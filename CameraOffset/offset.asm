
; in
extern CameraOffset : xmmword
extern CollisionOffset : xmmword

.data
	iCollision byte 0

.code
	SetCameraOffset proc
		addps xmm1, [CameraOffset]
		ret
	SetCameraOffset endp

	SetCollisionOffset proc

		; skip until every 3rd call
		;inc [iCollision]
		;cmp [iCollision], 3
		;jl return

		;mov [iCollision], 0
		addps xmm0, [CollisionOffset]
		movaps [rsp+58h], xmm0
	return:
		ret
	SetCollisionOffset endp
end