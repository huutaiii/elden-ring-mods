extern ReadCameraData : proto
extern CalcCameraOffset : proto

; out
extern Frametime : dword
extern CamBaseAddr : qword
extern CamSettingsPtr : qword

extern InteractPtr : qword

extern CritAnimElapsed : word

.code
	GetCameraData proc

		mov [CamBaseAddr], rsi

		push rax
		push rcx
		push rdx
		push r8
		push r9
		lea rsp, [rsp-10h]
		movaps [rsp], xmm15

		call ReadCameraData
		call CalcCameraOffset

		movaps xmm15, [rsp]
		lea rsp, [rsp+10h]
		pop r9
		pop r8
		pop rdx
		pop rcx
		pop rax

		ret
	GetCameraData endp

	GetCameraSettings proc
		mov [CamSettingsPtr], rax
		ret
	GetCameraSettings endp

	GetFrametime proc
		movss [Frametime], xmm1
		ret
	GetFrametime endp

	GetInteractState proc
		lea rcx, [rsp+8]
		push rax

		mov rax, [rcx+4D0h]
		mov [InteractPtr], rax

		pop rax
		ret
	GetInteractState endp

	SetCrit proc
		mov [CritAnimElapsed], 0
		ret
	SetCrit endp
end