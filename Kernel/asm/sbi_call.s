.section .text

.global sbi_call

# a7 SBI extension ID
# a6 SBI Function ID
# a1 second param
# a0 first param
sbi_call:
    ecall
    ret