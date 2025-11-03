// gcc -std=c11 -O2 -Wall -Wextra main.c my_assembler.c -o a.exe

#include "my_assembler.h"

int main(int argc, char **argv)
{
  // 인자 확인: inst.data와 입력 소스 경로 필요
  if (argc < 3)
  {
    fprintf(stderr, "Usage: %s <inst.data.txt> <input.sic>\n", argv[0]);
    return 1;
  }

  const char *inst_path = argv[1];
  const char *src_path = argv[2];

  // 1) 명령어 테이블 로드
  if (load_inst_table(inst_path) <= 0)
  {
    fprintf(stderr, "[ERROR] Failed to load instruction table from %s\n", inst_path);
    return 2;
  }

  // 2) 입력 소스 로드
  if (load_input(src_path) <= 0)
  {
    fprintf(stderr, "[ERROR] Failed to load input source from %s\n", src_path);
    return 3;
  }

  // 3) 토큰화
  if (tokenize_all() <= 0)
  {
    fprintf(stderr, "[ERROR] Tokenization failed or no lines.\n");
    return 4;
  }

  // 4) PASS1로 심볼테이블과 라인 주소를 만들고, Fig 2.2 스타일 리스트 출력
  if (pass1_build_symtab() != 0)
  {
    fprintf(stderr, "[ERROR] PASS1 failed.\n");
    return 5;
  }
  print_object_listing(stdout);
  return 0;
}
