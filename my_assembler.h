//
// my_assembler.h
//
// 목적: 학부생 수준에서 가장 단순하고 직관적인 SIC 어셈블리 토큰 파서/매퍼의 헤더 파일
//       - 주어진 inst.data.txt(명령어 테이블)를 읽어들여 mnemonic → opcode/format/피연산자 수를 매핑
//       - 입력 소스코드 각 라인을 label/operator/operand/comment로 나누어 토큰화
//       - 주석 라인(맨 앞이 '.')은 출력하지 않아도 됨(선택) → 여기서는 출력에서 제외
//       - 각 명령 라인 옆에 OP/FORMAT/OPERANDS 정보를 덧붙여 출력
//
// 최대한 단순한 규칙으로 동작하도록 작성되었으며, 코드 곳곳에 왜 그렇게 했는지 설명을 자세히 담았습니다.
//

#ifndef MY_ASSEMBLER_H
#define MY_ASSEMBLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// 과제에서 제시된 상수/배열 크기
#define MAX_LINES 5000 // 입력 소스코드 최대 라인 수
#define MAX_OPERAND 3  // 한 명령의 최대 피연산자 수
#define MAX_INST 256   // 명령어 테이블 최대 엔트리 수

// 입력 소스코드 라인 보관 테이블
extern char *input_data[MAX_LINES];
extern int line_num; // 읽어들인 실제 라인 수

// 라인을 토큰 단위로 관리하기 위한 구조체
typedef struct token_unit
{
  char *label;                   // 명령어 라인 중 label (없으면 NULL)
  char *operator;                // 명령어 라인 중 operator (mnemonic/지시자)
  char operand[MAX_OPERAND][20]; // 명령어 라인 중 operand들(간단히 20자 제한)
  char comment[100];             // 주석 텍스트
  int operand_count;             // 파싱된 피연산자 수
  bool is_comment_line;          // 라인의 첫 비공백이 '.' 인지(전부 주석 라인 여부)
  char *original;                // 원본 라인 문자열(출력시 참고용)
  int addr;                      // 라인의 시작 주소
  char obj[16];                  // 생성된 객체코드(최대 6~8 자리 정도, 넉넉히)
  bool has_object;               // 객체코드가 있는지(RESB/RESW/START/END 등은 false)
} token;

// Instruction(명령어) 테이블 관리 구조체 (과제 명세에 맞춤)
typedef struct inst_unit
{
  char str[10];     // mnemonic 문자열 (대문자 보관)
  unsigned char op; // OPCODE(헥사 값)
  int format;       // instruction 형식(1/2/3/0: 0은 지시자 등)
  int ops;          // 피연산자 개수(단순 규칙으로 계산)
  char type[4];     // inst.data의 두 번째 열("M", "RR", "R", "RN", "-", "N")
} inst;

extern inst *inst_table[MAX_INST];
extern int inst_index; // 로드된 명령어 개수

// 공용 유틸 함수
char *str_trim(char *s);         // 좌우 공백 제거
char *str_ltrim(char *s);        // 좌측 공백 제거
char *str_rtrim(char *s);        // 우측 공백 제거
void str_upper_inplace(char *s); // 대문자 변환(영문자만)
char *str_dup(const char *s);    // strdup 대체

// 인스트럭션 테이블 로드/검색
int load_inst_table(const char *path); // inst.data.txt 읽어서 inst_table 채움
inst *find_inst(const char *mnemonic); // 대소문자 구분 없이 검색

// 입력/토큰화/출력
int load_input(const char *path);    // 소스코드 파일 읽어 input_data/line_num 채움
int tokenize_all(void);              // input_data 전체를 token_table에 토큰화
void print_mapped_output(FILE *out); // 토큰 기반으로 매핑 결과를 출력

// 전역 토큰 테이블
extern token *token_table[MAX_LINES];

// --- 심볼 테이블과 2-패스용 선언 ---
typedef struct sym_unit
{
  char name[32]; // 라벨 이름(대문자 보관)
  int addr;      // 절대 주소(16진 주소)
} sym;

extern sym sym_table[MAX_LINES];
extern int sym_count;
extern int start_addr; // START 주소(기준 주소)

int pass1_build_symtab(void);      // LOCCTR를 계산하며 라벨의 주소를 기록
void print_object_listing(FILE *); // 각 라인 주소와 객체코드(가능한 경우)를 출력

#endif // MY_ASSEMBLER_H
