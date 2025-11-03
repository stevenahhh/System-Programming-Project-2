#include "my_assembler.h"
#include <ctype.h>

// 전역 변수 정의 (헤더에서 extern 선언됨)
char *input_data[MAX_LINES];
int line_num = 0;

inst *inst_table[MAX_INST];
int inst_index = 0;

token *token_table[MAX_LINES];

sym sym_table[MAX_LINES];
int sym_count = 0;
int start_addr = 0;

// ========== 유틸리티 함수 ==========

// 왼쪽 공백 제거
char *str_ltrim(char *s)
{
  if (!s)
    return s;
  while (*s && isspace((unsigned char)*s))
    s++;
  return s;
}

// 오른쪽 공백 제거
char *str_rtrim(char *s)
{
  if (!s)
    return s;
  char *end = s + strlen(s) - 1;
  while (end >= s && isspace((unsigned char)*end))
  {
    *end = '\0';
    end--;
  }
  return s;
}

// 좌우 공백 제거
char *str_trim(char *s)
{
  return str_rtrim(str_ltrim(s));
}

// 문자열을 대문자로 변환 (in-place)
void str_upper_inplace(char *s)
{
  if (!s)
    return;
  for (int i = 0; s[i]; i++)
  {
    s[i] = (char)toupper((unsigned char)s[i]);
  }
}

// strdup 대체 (malloc 사용)
char *str_dup(const char *s)
{
  if (!s)
    return NULL;
  size_t len = strlen(s);
  char *dup = (char *)malloc(len + 1);
  if (!dup)
    return NULL;
  strcpy(dup, s);
  return dup;
}

// ========== 명령어 테이블 관련 함수 ==========

// inst.data.txt 파일을 읽어서 inst_table 채우기
// 형식: MNEMONIC\tTYPE\tFORMAT\tOPCODE (탭으로 구분)
int load_inst_table(const char *path)
{
  FILE *fp = fopen(path, "r");
  if (!fp)
  {
    fprintf(stderr, "[ERROR] Cannot open inst.data.txt: %s\n", path);
    return -1;
  }

  char line[256];
  inst_index = 0;

  while (fgets(line, sizeof(line), fp))
  {
    // 빈 줄이나 주석은 스킵
    char *trimmed = str_trim(line);
    if (strlen(trimmed) == 0)
      continue;

    // 탭으로 분리: MNEMONIC TYPE FORMAT OPCODE
    char mnemonic[32], type[8], opcode_str[8];
    int format;

    // sscanf로 파싱 (탭 구분)
    int n = sscanf(line, "%s\t%s\t%d\t%s", mnemonic, type, &format, opcode_str);
    if (n < 4)
      continue; // 파싱 실패 시 스킵

    // inst 구조체 생성
    inst *new_inst = (inst *)malloc(sizeof(inst));
    if (!new_inst)
      continue;

    strncpy(new_inst->str, mnemonic, sizeof(new_inst->str) - 1);
    new_inst->str[sizeof(new_inst->str) - 1] = '\0';
    str_upper_inplace(new_inst->str); // 대문자 변환

    strncpy(new_inst->type, type, sizeof(new_inst->type) - 1);
    new_inst->type[sizeof(new_inst->type) - 1] = '\0';

    new_inst->format = format;

    // OPCODE는 16진수로 파싱
    unsigned int op_val = 0;
    sscanf(opcode_str, "%X", &op_val);
    new_inst->op = (unsigned char)op_val;

    // 피연산자 개수 판단 (간단한 규칙)
    // M(메모리 참조)는 일반적으로 1개, RR(레지스터-레지스터)는 2개, R/N은 1개 등
    if (strcmp(new_inst->type, "M") == 0)
    {
      // 메모리 명령어는 대부분 피연산자 1개
      new_inst->ops = 1;
    }
    else if (strcmp(new_inst->type, "RR") == 0)
    {
      new_inst->ops = 2; // 레지스터 2개
    }
    else if (strcmp(new_inst->type, "R") == 0 || strcmp(new_inst->type, "N") == 0 ||
             strcmp(new_inst->type, "RN") == 0)
    {
      new_inst->ops = 1;
    }
    else if (strcmp(new_inst->type, "-") == 0)
    {
      // RSUB, LTORG 등: 피연산자 없음
      new_inst->ops = 0;
    }
    else
    {
      new_inst->ops = 0;
    }

    // 테이블에 추가
    inst_table[inst_index++] = new_inst;

    if (inst_index >= MAX_INST)
      break;
  }

  fclose(fp);
  return inst_index;
}

// 명령어 검색 (대소문자 무시)
inst *find_inst(const char *mnemonic)
{
  if (!mnemonic)
    return NULL;

  char upper_mnemonic[32];
  strncpy(upper_mnemonic, mnemonic, sizeof(upper_mnemonic) - 1);
  upper_mnemonic[sizeof(upper_mnemonic) - 1] = '\0';
  str_upper_inplace(upper_mnemonic);

  for (int i = 0; i < inst_index; i++)
  {
    if (strcmp(inst_table[i]->str, upper_mnemonic) == 0)
    {
      return inst_table[i];
    }
  }
  return NULL;
}

// ========== 입력 소스코드 로드 ==========

// 소스코드 파일을 라인별로 읽어 input_data에 저장
int load_input(const char *path)
{
  FILE *fp = fopen(path, "r");
  if (!fp)
  {
    fprintf(stderr, "[ERROR] Cannot open input file: %s\n", path);
    return -1;
  }

  char line[512];
  line_num = 0;

  while (fgets(line, sizeof(line), fp) && line_num < MAX_LINES)
  {
    // 개행 문자 제거
    char *nl = strchr(line, '\n');
    if (nl)
      *nl = '\0';
    nl = strchr(line, '\r');
    if (nl)
      *nl = '\0';

    // 라인을 복사하여 저장
    input_data[line_num++] = str_dup(line);
  }

  fclose(fp);
  return line_num;
}

// ========== 토큰화 함수 ==========

// 주어진 라인을 파싱하여 token 구조체 생성
token *tokenize_line(const char *line_str)
{
  if (!line_str)
    return NULL;

  token *tok = (token *)calloc(1, sizeof(token));
  if (!tok)
    return NULL;

  tok->original = str_dup(line_str);
  tok->operand_count = 0;
  tok->is_comment_line = false;
  tok->has_object = false;
  tok->addr = 0;
  strcpy(tok->obj, "");

  // 빈 줄 처리
  char *dup_for_trim = str_dup(line_str);
  char *trimmed = str_trim(dup_for_trim);
  if (strlen(trimmed) == 0)
  {
    free(dup_for_trim);
    return tok;
  }

  // 주석 라인 체크 ('.'로 시작)
  if (trimmed[0] == '.')
  {
    tok->is_comment_line = true;
    free(dup_for_trim);
    return tok;
  }
  free(dup_for_trim);

  // 라인 복사 (파싱용)
  char work[512];
  strncpy(work, line_str, sizeof(work) - 1);
  work[sizeof(work) - 1] = '\0';

  // SIC 어셈블리 형식: [LABEL] OPERATOR [OPERAND1,OPERAND2,...] [COMMENT]
  // 간단한 파싱: 탭/공백으로 분리

  char *ptr = work;

  // 첫 번째 비공백 문자 찾기
  while (*ptr && isspace((unsigned char)*ptr))
    ptr++;

  if (*ptr == '\0')
    return tok; // 빈 줄

  // Label 또는 Operator 파싱
  // Label은 맨 앞 칼럼에서 시작하고, 공백/탭이 없으면 Label
  // Operator는 공백/탭 뒤에 나옴
  // 간단하게: 첫 토큰이 공백 없이 시작하면 Label, 아니면 Operator

  bool has_label = (line_str[0] != ' ' && line_str[0] != '\t' && line_str[0] != '\0');

  char *tokens[10];
  int token_count = 0;

  // 공백/탭으로 토큰 분리
  char *token = strtok(ptr, " \t");
  while (token && token_count < 10)
  {
    tokens[token_count++] = token;
    token = strtok(NULL, " \t");
  }

  int idx = 0;

  // Label 처리
  if (has_label && token_count > 0)
  {
    tok->label = str_dup(tokens[idx++]);
  }

  // Operator 처리
  if (idx < token_count)
  {
    tok->operator = str_dup(tokens[idx++]);
  }

  // Operand 처리 (콤마로 구분될 수 있음)
  if (idx < token_count)
  {
    // Operand는 콤마로 구분
    char operand_str[256] = "";
    for (int i = idx; i < token_count; i++)
    {
      strcat(operand_str, tokens[i]);
      if (i < token_count - 1)
        strcat(operand_str, " ");
    }

    // 콤마로 분리
    char *op_ptr = operand_str;
    char *op_token = strtok(op_ptr, ",");
    while (op_token && tok->operand_count < MAX_OPERAND)
    {
      char *trimmed_op = str_trim(op_token);
      strncpy(tok->operand[tok->operand_count++], trimmed_op, 19);
      tok->operand[tok->operand_count - 1][19] = '\0';
      op_token = strtok(NULL, ",");
    }
  }

  return tok;
}

// 모든 입력 라인을 토큰화
int tokenize_all(void)
{
  for (int i = 0; i < line_num; i++)
  {
    token_table[i] = tokenize_line(input_data[i]);
  }
  return line_num;
}

// ========== 심볼 테이블 및 PASS1 ==========

// 심볼 추가
void add_symbol(const char *name, int addr)
{
  if (sym_count >= MAX_LINES)
    return;

  strncpy(sym_table[sym_count].name, name, sizeof(sym_table[sym_count].name) - 1);
  sym_table[sym_count].name[sizeof(sym_table[sym_count].name) - 1] = '\0';
  str_upper_inplace(sym_table[sym_count].name);
  sym_table[sym_count].addr = addr;
  sym_count++;
}

// 심볼 검색
int find_symbol(const char *name)
{
  char upper_name[32];
  strncpy(upper_name, name, sizeof(upper_name) - 1);
  upper_name[sizeof(upper_name) - 1] = '\0';
  str_upper_inplace(upper_name);

  for (int i = 0; i < sym_count; i++)
  {
    if (strcmp(sym_table[i].name, upper_name) == 0)
    {
      return sym_table[i].addr;
    }
  }
  return -1; // 찾지 못함
}

// PASS1: 심볼 테이블 생성 및 주소 계산
int pass1_build_symtab(void)
{
  int locctr = 0;
  start_addr = 0;

  for (int i = 0; i < line_num; i++)
  {
    token *tok = token_table[i];
    if (!tok)
      continue;

    // 주석 라인은 스킵
    if (tok->is_comment_line)
      continue;

    // Operator가 없으면 스킵
    if (!tok->operator)
      continue;

    char op_upper[32];
    strncpy(op_upper, tok->operator, sizeof(op_upper) - 1);
    op_upper[sizeof(op_upper) - 1] = '\0';
    str_upper_inplace(op_upper);

    // START 지시자 처리
    if (strcmp(op_upper, "START") == 0)
    {
      if (tok->operand_count > 0)
      {
        // 16진수 파싱
        sscanf(tok->operand[0], "%X", (unsigned int *)&locctr);
        start_addr = locctr;
      }
      tok->addr = locctr;

      // Label이 있으면 심볼 테이블에 추가
      if (tok->label)
      {
        add_symbol(tok->label, locctr);
      }
      continue;
    }

    // 현재 라인의 주소 설정
    tok->addr = locctr;

    // Label이 있으면 심볼 테이블에 추가
    if (tok->label)
    {
      add_symbol(tok->label, locctr);
    }

    // END 지시자 처리
    if (strcmp(op_upper, "END") == 0)
    {
      break;
    }

    // 주소 증가량 계산
    int increment = 0;

    if (strcmp(op_upper, "WORD") == 0)
    {
      increment = 3;
    }
    else if (strcmp(op_upper, "RESW") == 0)
    {
      if (tok->operand_count > 0)
      {
        int count = atoi(tok->operand[0]);
        increment = count * 3;
      }
    }
    else if (strcmp(op_upper, "RESB") == 0)
    {
      if (tok->operand_count > 0)
      {
        int count = atoi(tok->operand[0]);
        increment = count;
      }
    }
    else if (strcmp(op_upper, "BYTE") == 0)
    {
      // BYTE 지시자: C'...' 또는 X'...'
      if (tok->operand_count > 0)
      {
        char *operand = tok->operand[0];
        if (operand[0] == 'C' || operand[0] == 'c')
        {
          // C'문자열': 문자열 길이
          char *start = strchr(operand, '\'');
          char *end = strrchr(operand, '\'');
          if (start && end && end > start)
          {
            increment = (int)(end - start - 1);
          }
        }
        else if (operand[0] == 'X' || operand[0] == 'x')
        {
          // X'16진수': 16진수 길이 / 2
          char *start = strchr(operand, '\'');
          char *end = strrchr(operand, '\'');
          if (start && end && end > start)
          {
            int hex_len = (int)(end - start - 1);
            increment = (hex_len + 1) / 2;
          }
        }
      }
    }
    else
    {
      // 명령어 검색
      inst *instruction = find_inst(op_upper);
      if (instruction)
      {
        increment = instruction->format;
      }
    }

    locctr += increment;
  }

  return 0;
}

// ========== PASS2: 객체코드 생성 ==========

// 객체코드 생성
void generate_object_code(token *tok)
{
  if (!tok || !tok->operator)
    return;

  char op_upper[32];
  strncpy(op_upper, tok->operator, sizeof(op_upper) - 1);
  op_upper[sizeof(op_upper) - 1] = '\0';
  str_upper_inplace(op_upper);

  // 명령어 검색
  inst *instruction = find_inst(op_upper);

  if (instruction && instruction->format > 0)
  // 참고 출처 Claude: BYTE와 WORD가 inst.data.txt에 있고 format이 0입니다. 이것들은 지시자이므로 명령어가 아닙니다. generate_object_code 함수에서 format이 0인 경우를 처리해야 합니다.
  {
    // Format 3 명령어 처리
    if (instruction->format == 3)
    {
      unsigned int opcode = instruction->op;
      int target_addr = 0;
      bool indexed = false;

      // 피연산자 처리
      if (tok->operand_count > 0)
      {
        char *operand = tok->operand[0];

        // 인덱싱 체크 (예: BUFFER,X)
        if (tok->operand_count == 2)
        {
          char x_upper[8];
          strncpy(x_upper, tok->operand[1], sizeof(x_upper) - 1);
          x_upper[sizeof(x_upper) - 1] = '\0';
          str_upper_inplace(x_upper);
          if (strcmp(x_upper, "X") == 0)
          {
            indexed = true;
          }
        }

        // 심볼 주소 찾기
        target_addr = find_symbol(operand);
        if (target_addr < 0)
          target_addr = 0; // 찾지 못하면 0
      }

      // 객체코드 생성: OPCODE(8bit) + nixbpe(6bit) + disp(12bit)
      // 간단히: OPCODE + address (24bit)
      unsigned int obj_code = (opcode << 16) | (target_addr & 0xFFFF);

      // 인덱싱 비트 설정 (bit 15)
      if (indexed)
      {
        obj_code |= 0x8000;
      }

      sprintf(tok->obj, "%06X", obj_code);
      tok->has_object = true;
    }
    else if (instruction->format == 1)
    {
      // Format 1: OPCODE만 (8bit)
      sprintf(tok->obj, "%02X", instruction->op);
      tok->has_object = true;
    }
    else if (instruction->format == 2)
    {
      // Format 2: OPCODE(8bit) + r1(4bit) + r2(4bit)
      // 간단 구현: OPCODE + 00
      sprintf(tok->obj, "%04X", (instruction->op << 8));
      tok->has_object = true;
    }
  }
  else
  {
    // 지시자 처리 (format == 0 또는 명령어 테이블에 없음)
    if (strcmp(op_upper, "WORD") == 0)
    {
      if (tok->operand_count > 0)
      {
        int value = atoi(tok->operand[0]);
        sprintf(tok->obj, "%06X", value & 0xFFFFFF);
        tok->has_object = true;
      }
    }
    else if (strcmp(op_upper, "BYTE") == 0)
    {
      if (tok->operand_count > 0)
      {
        char *operand = tok->operand[0];
        if (operand[0] == 'C' || operand[0] == 'c')
        {
          // C'문자열': ASCII 코드
          char *start = strchr(operand, '\'');
          char *end = strrchr(operand, '\'');
          if (start && end && end > start)
          {
            char str[128];
            strncpy(str, start + 1, end - start - 1);
            str[end - start - 1] = '\0';

            // ASCII 코드를 16진수로 변환
            tok->obj[0] = '\0';
            for (int i = 0; str[i] && i < 16; i++)
            {
              char hex[3];
              sprintf(hex, "%02X", (unsigned char)str[i]);
              strcat(tok->obj, hex);
            }
            tok->has_object = true;
          }
        }
        else if (operand[0] == 'X' || operand[0] == 'x')
        {
          // X'16진수': 그대로 복사
          char *start = strchr(operand, '\'');
          char *end = strrchr(operand, '\'');
          if (start && end && end > start)
          {
            strncpy(tok->obj, start + 1, end - start - 1);
            tok->obj[end - start - 1] = '\0';
            tok->has_object = true;
          }
        }
      }
    }
  }
}

// ========== 출력 함수 ==========

void print_object_listing(FILE *out)
{
  // PASS2: 객체코드 생성
  for (int i = 0; i < line_num; i++)
  {
    token *tok = token_table[i];
    if (!tok)
      continue;

    // 주석 라인은 출력하지 않음
    if (tok->is_comment_line)
      continue;

    if (!tok->operator)
      continue;

    generate_object_code(tok);
  }

  // 출력
  for (int i = 0; i < line_num; i++)
  {
    token *tok = token_table[i];
    if (!tok)
      continue;

    // 주석 라인은 출력하지 않음
    if (tok->is_comment_line)
      continue;

    if (!tok->operator)
      continue;

    // 주소 출력
    fprintf(out, "%04X", tok->addr);

    // Label 출력 (8칸 정렬)
    if (tok->label)
    {
      fprintf(out, "\t%s", tok->label);
    }
    else
    {
      fprintf(out, "\t");
    }

    // Operator 출력
    fprintf(out, "\t%s", tok->operator);

    // Operand 출력
    if (tok->operand_count > 0)
    {
      fprintf(out, "\t");
      for (int j = 0; j < tok->operand_count; j++)
      {
        fprintf(out, "%s", tok->operand[j]);
        if (j < tok->operand_count - 1)
        {
          fprintf(out, ",");
        }
      }
    }

    // 객체코드 출력 (있는 경우)
    if (tok->has_object)
    {
      fprintf(out, "\t\t\t%s", tok->obj);
    }

    fprintf(out, "\n");
  }
}

// 단순 매핑 출력
void print_mapped_output(FILE *out)
{
  for (int i = 0; i < line_num; i++)
  {
    token *tok = token_table[i];
    if (!tok)
      continue;

    // 주석 라인 스킵
    if (tok->is_comment_line)
      continue;

    // 원본 라인 출력
    if (tok->original)
    {
      fprintf(out, "%s", tok->original);
    }

    // Operator가 있으면 명령어 정보 출력
    if (tok->operator)
    {
      inst *instruction = find_inst(tok->operator);
      if (instruction)
      {
        fprintf(out, "\t\t[OP=%02X, FORMAT=%d, OPERANDS=%d]",
                instruction->op, instruction->format, instruction->ops);
      }
    }

    fprintf(out, "\n");
  }
}
