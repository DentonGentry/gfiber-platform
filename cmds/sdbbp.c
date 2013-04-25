/*
 * Test that the mips 'sdbbp' instruction doesn't insta-crash the machine.
 * Not that anybody I know would build a machine like that.
 */

int main(void) {
  // non-mips doesn't have this instruction, so I guess it passes :)
#ifdef __mips__
  __asm__("sdbbp");
#endif
  return 0;
}