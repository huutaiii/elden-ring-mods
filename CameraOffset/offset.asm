
; in
extern CameraOffset : xmmword
extern CollisionOffset : xmmword
extern RetractCollisionOffset : xmmword

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
		subps xmm2, [CollisionOffset]
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
end