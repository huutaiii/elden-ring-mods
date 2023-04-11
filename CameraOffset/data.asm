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

		; backup volatile registers since we're hook into the middle of a function
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

		lea rsp, [rsp-20h]		; shadow space for function calls
		call ReadCameraData
		call CalcCameraOffset
		lea rsp, [rsp+20h]
		
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
		push rcx
		lea rcx, [rsp+16]
		push rax

		mov rax, [rcx+4D0h]
		mov [InteractPtr], rax

		pop rax
		pop rcx
		ret
	GetInteractState endp

	SetCrit proc
		mov [CritAnimElapsed], 0
		ret
	SetCrit endp
end