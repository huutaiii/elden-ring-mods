extern ReadCameraData : proto
extern CalcCameraOffset : proto

; out
extern CamBaseAddr : qword
extern Frametime : dword

extern bIsTalking : byte

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
end