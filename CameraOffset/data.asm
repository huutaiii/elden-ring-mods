extern ReadCameraData : proto
extern CalcCameraOffset : proto

; out
extern Frametime : dword
extern CamBaseAddr : qword
extern CamSettingsPtr : qword

extern bIsTalking : byte
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

		call ReadCameraData
		call CalcCameraOffset

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

	GetNPCState proc
		test rdi, rdi
		je GetNPCState_return
		push rax
		xor al, al
		mov al, [rsi+28h]
		and al, 20h
		cmp al, 0
		setz al
		mov [bIsTalking], al
		pop rax

	GetNPCState_return:
		ret
	GetNPCState endp

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