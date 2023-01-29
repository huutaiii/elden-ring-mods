extern ReadCameraData : proto
extern CalcCameraOffset : proto

; out
extern CamBaseAddr : qword
extern Frametime : dword

.code
	GetCameraData proc

		mov [CamBaseAddr], rsi

		push rax
		push rcx
		push rdx
		push r8
		push r9

		call ReadCameraData
		call CalcCameraOffset

		pop r9
		pop r8
		pop rdx
		pop rcx
		pop rax

		ret
	GetCameraData endp

	GetFrametime proc
		movss [Frametime], xmm1
		ret
	GetFrametime endp
end