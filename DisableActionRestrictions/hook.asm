
.data
	extern ReturnAddress : qword

.code

	OverrideRestrictedArea proc
		push rax
		mov rax,0000000f00000000h
		cmp rdx,rax
		jne continue
		lea rsp,[rsp+8]
		xor rax,rax
		ret

	continue:
		pop rax
		jmp [ReturnAddress]

	OverrideRestrictedArea endp

end