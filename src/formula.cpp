#include "formula.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {

enum Code : uint8_t {
  NUM, VAR_T, VAR_I, VAR_X, VAR_Y,
  ADD, SUB, MUL, DIV, MOD, POW,
  LT, GT, LE, GE, EQ, NE,
  BAND, BOR, BXOR, SHL, SHR,
  NEG, NOT, BNOT,
  CALL,
  JZ,    // pop; jump if zero
  JMP,
  AND,   // a && b, both already evaluated: 1 if both non-zero
  OR,
};

enum Fn : uint8_t { SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2, ABS, SQRT, FLOOR, CEIL, ROUND, SIGN, MIN, MAX, POWF, HYPOT, LOG, EXP, RANDOM };
struct FnInfo {
  const char *name;
  Fn fn;
  int8_t args;  // -1: one or more
};
const FnInfo FUNCTIONS[] = {
    {"sin", SIN, 1},     {"cos", COS, 1},     {"tan", TAN, 1},     {"asin", ASIN, 1}, {"acos", ACOS, 1},
    {"atan", ATAN, 1},   {"atan2", ATAN2, 2}, {"abs", ABS, 1},     {"sqrt", SQRT, 1}, {"floor", FLOOR, 1},
    {"ceil", CEIL, 1},   {"round", ROUND, 1}, {"sign", SIGN, 1},   {"min", MIN, -1},  {"max", MAX, -1},
    {"pow", POWF, 2},    {"hypot", HYPOT, -1}, {"log", LOG, 1},    {"exp", EXP, 1},   {"random", RANDOM, 0},
};

const int MAX_STACK = 32;
const size_t MAX_CODE = 256;

int32_t toInt(float v) { return isfinite(v) ? (int32_t)(int64_t)v : 0; }

}  // namespace

// Recursive descent, JavaScript precedence (lowest first):
// ?: , || , && , | , ^ , & , == != , < > <= >= , << >> , + - , * / % ,
// unary - ! ~ , ** (right-associative), primary.
class FormulaParser {
 public:
  FormulaParser(const String &text, std::vector<Formula::Op> &code) : s_(text), code_(code) {}

  bool parse(String &error) {
    skip();
    ternary();
    if (!failed_ && pos_ < (int)s_.length()) fail("carattere inatteso");
    if (!failed_ && maxDepth_ > MAX_STACK) fail("formula troppo annidata");
    if (!failed_ && code_.size() > MAX_CODE) fail("formula troppo lunga");
    if (failed_) {
      error = message_ + " (posizione " + String(errorPos_ + 1) + ")";
      code_.clear();
      return false;
    }
    return true;
  }

 private:
  const String &s_;
  std::vector<Formula::Op> &code_;
  int pos_ = 0, depth_ = 0, maxDepth_ = 0, nest_ = 0;
  bool failed_ = false;
  String message_;
  int errorPos_ = 0;

  void fail(const char *msg) {
    if (failed_) return;
    failed_ = true;
    message_ = msg;
    errorPos_ = pos_;
  }
  char peek(int k = 0) const { return pos_ + k < (int)s_.length() ? s_[pos_ + k] : 0; }
  void skip() {
    while (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r') pos_++;
  }
  bool eat(const char *tok) {
    const int n = strlen(tok);
    if (strncmp(s_.c_str() + pos_, tok, n) != 0) return false;
    pos_ += n;
    skip();
    return true;
  }
  // Like eat() but not when the operator continues (e.g. "&" in "&&").
  bool eatOp(const char *tok, const char *notFollowedBy) {
    const int n = strlen(tok);
    if (strncmp(s_.c_str() + pos_, tok, n) != 0) return false;
    if (notFollowedBy && peek(n) && strchr(notFollowedBy, peek(n))) return false;
    pos_ += n;
    skip();
    return true;
  }
  void emit(uint8_t c, float v = 0, uint8_t arg = 0) {
    code_.push_back({c, arg, 0, v});
    // Stack depth bookkeeping: values pushed minus popped.
    if (c == NUM || c <= VAR_Y) depth_++;
    else if (c >= ADD && c <= SHR) depth_--;
    else if (c == AND || c == OR) depth_--;
    else if (c == CALL) depth_ += 1 - arg;
    else if (c == JZ) depth_--;
    if (depth_ > maxDepth_) maxDepth_ = depth_;
  }

  void ternary() {
    // Guards the C stack: each level of brackets recurses through here.
    if (++nest_ > 24) return fail("troppe parentesi una dentro l'altra");
    ternaryBody();
    nest_--;
  }
  void ternaryBody() {
    logicalOr();
    if (!eat("?")) return;
    const size_t jz = code_.size();
    emit(JZ);
    ternary();
    const size_t jmp = code_.size();
    emit(JMP);
    depth_--;  // only one branch's value stays
    if (!eat(":")) return fail("manca «:» dopo «?»");
    code_[jz].jump = code_.size();
    ternary();
    code_[jmp].jump = code_.size();
  }
  void logicalOr() {
    logicalAnd();
    while (!failed_ && eat("||")) {
      logicalAnd();
      emit(OR);
    }
  }
  void logicalAnd() {
    bitOr();
    while (!failed_ && eat("&&")) {
      bitOr();
      emit(AND);
    }
  }
  void bitOr() {
    bitXor();
    while (!failed_ && eatOp("|", "|")) {
      bitXor();
      emit(BOR);
    }
  }
  void bitXor() {
    bitAnd();
    while (!failed_ && eat("^")) {
      bitAnd();
      emit(BXOR);
    }
  }
  void bitAnd() {
    equality();
    while (!failed_ && eatOp("&", "&")) {
      equality();
      emit(BAND);
    }
  }
  void equality() {
    relational();
    for (;;) {
      if (failed_) return;
      if (eat("===") || eat("==")) {
        relational();
        emit(EQ);
      } else if (eat("!==") || eat("!=")) {
        relational();
        emit(NE);
      } else {
        return;
      }
    }
  }
  void relational() {
    shift();
    for (;;) {
      if (failed_) return;
      if (eat("<=")) {
        shift();
        emit(LE);
      } else if (eat(">=")) {
        shift();
        emit(GE);
      } else if (eatOp("<", "<")) {
        shift();
        emit(LT);
      } else if (eatOp(">", ">")) {
        shift();
        emit(GT);
      } else {
        return;
      }
    }
  }
  void shift() {
    additive();
    for (;;) {
      if (failed_) return;
      if (eat("<<")) {
        additive();
        emit(SHL);
      } else if (eat(">>>") || eat(">>")) {
        additive();
        emit(SHR);
      } else {
        return;
      }
    }
  }
  void additive() {
    multiplicative();
    for (;;) {
      if (failed_) return;
      if (eat("+")) {
        multiplicative();
        emit(ADD);
      } else if (eat("-")) {
        multiplicative();
        emit(SUB);
      } else {
        return;
      }
    }
  }
  void multiplicative() {
    unary();
    for (;;) {
      if (failed_) return;
      if (eatOp("*", "*")) {
        unary();
        emit(MUL);
      } else if (eat("/")) {
        unary();
        emit(DIV);
      } else if (eat("%")) {
        unary();
        emit(MOD);
      } else {
        return;
      }
    }
  }
  void unary() {
    if (eat("-")) {
      unary();
      emit(NEG);
    } else if (eat("+")) {
      unary();
    } else if (eatOp("!", "=")) {
      unary();
      emit(NOT);
    } else if (eat("~")) {
      unary();
      emit(BNOT);
    } else {
      power();
    }
  }
  void power() {
    primary();
    if (!failed_ && eat("**")) {
      unary();  // right-associative, and allows 2**-1
      emit(POW);
    }
  }
  void primary() {
    if (failed_) return;
    const char c = peek();
    if ((c >= '0' && c <= '9') || c == '.') {
      char *end;
      const float v = strtof(s_.c_str() + pos_, &end);
      if (end == s_.c_str() + pos_) return fail("numero non valido");
      pos_ = end - s_.c_str();
      skip();
      emit(NUM, v);
      return;
    }
    if (eat("(")) {
      ternary();
      if (!failed_ && !eat(")")) fail("manca «)»");
      return;
    }
    if (isalpha((uint8_t)c) || c == '_') {
      const int start = pos_;
      while (isalnum((uint8_t)peek()) || peek() == '_' || peek() == '.') pos_++;
      String name = s_.substring(start, pos_);
      if (name.startsWith("Math.")) name = name.substring(5);
      skip();
      if (name == "t") return emit(VAR_T);
      if (name == "i") return emit(VAR_I);
      if (name == "x") return emit(VAR_X);
      if (name == "y") return emit(VAR_Y);
      if (name == "PI") return emit(NUM, (float)M_PI);
      if (name == "true") return emit(NUM, 1);
      if (name == "false") return emit(NUM, 0);
      for (const FnInfo &f : FUNCTIONS) {
        if (name != f.name) continue;
        if (!eat("(")) {
          pos_ = start;
          return fail("dopo il nome di una funzione serve «(»");
        }
        int args = 0;
        if (!eat(")")) {
          do {
            ternary();
            args++;
          } while (!failed_ && eat(","));
          if (!failed_ && !eat(")")) return fail("manca «)»");
        }
        if (failed_) return;
        if ((f.args >= 0 && args != f.args) || (f.args < 0 && args < 1)) {
          pos_ = start;
          return fail("numero di argomenti sbagliato");
        }
        emit(CALL, f.fn, args);
        return;
      }
      pos_ = start;
      return fail("nome sconosciuto (si usano t, i, x, y e le funzioni di Math)");
    }
    fail(c ? "manca un valore" : "la formula finisce troppo presto");
  }
};

bool Formula::compile(const String &text, String &error) {
  std::vector<Op> code;
  FormulaParser parser(text, code);
  if (!parser.parse(error)) return false;
  code_ = code;
  return true;
}

float Formula::eval(float t, float i, float x, float y) const {
  float st[MAX_STACK + 2];
  int sp = 0;
  const Op *ops = code_.data();
  const int n = code_.size();
  for (int pc = 0; pc < n; pc++) {
    const Op &op = ops[pc];
    switch (op.code) {
      case NUM: st[sp++] = op.value; break;
      case VAR_T: st[sp++] = t; break;
      case VAR_I: st[sp++] = i; break;
      case VAR_X: st[sp++] = x; break;
      case VAR_Y: st[sp++] = y; break;
      case NEG: st[sp - 1] = -st[sp - 1]; break;
      case NOT: st[sp - 1] = st[sp - 1] == 0; break;
      case BNOT: st[sp - 1] = ~toInt(st[sp - 1]); break;
      case JZ:
        if (st[--sp] == 0) pc = op.jump - 1;
        break;
      case JMP: pc = op.jump - 1; break;
      case CALL: {
        const int k = op.arg;
        float *a = st + sp - k;
        float r = 0;
        switch (op.value < 0 ? 0 : (int)op.value) {
          case SIN: r = sinf(a[0]); break;
          case COS: r = cosf(a[0]); break;
          case TAN: r = tanf(a[0]); break;
          case ASIN: r = asinf(a[0]); break;
          case ACOS: r = acosf(a[0]); break;
          case ATAN: r = atanf(a[0]); break;
          case ATAN2: r = atan2f(a[0], a[1]); break;
          case ABS: r = fabsf(a[0]); break;
          case SQRT: r = sqrtf(a[0]); break;
          case FLOOR: r = floorf(a[0]); break;
          case CEIL: r = ceilf(a[0]); break;
          case ROUND: r = floorf(a[0] + 0.5f); break;
          case SIGN: r = a[0] > 0 ? 1 : a[0] < 0 ? -1 : 0; break;
          case MIN:
            r = a[0];
            for (int j = 1; j < k; j++) r = fminf(r, a[j]);
            break;
          case MAX:
            r = a[0];
            for (int j = 1; j < k; j++) r = fmaxf(r, a[j]);
            break;
          case POWF: r = powf(a[0], a[1]); break;
          case HYPOT: {
            float s2 = 0;
            for (int j = 0; j < k; j++) s2 += a[j] * a[j];
            r = sqrtf(s2);
            break;
          }
          case LOG: r = logf(a[0]); break;
          case EXP: r = expf(a[0]); break;
          case RANDOM: r = (esp_random() >> 8) / 16777216.0f; break;
        }
        sp -= k;
        st[sp++] = r;
        break;
      }
      default: {
        const float b = st[--sp], a = st[sp - 1];
        float r;
        switch (op.code) {
          case ADD: r = a + b; break;
          case SUB: r = a - b; break;
          case MUL: r = a * b; break;
          case DIV: r = a / b; break;
          case MOD: r = fmodf(a, b); break;  // like JavaScript: the sign of a
          case POW: r = powf(a, b); break;
          case LT: r = a < b; break;
          case GT: r = a > b; break;
          case LE: r = a <= b; break;
          case GE: r = a >= b; break;
          case EQ: r = a == b; break;
          case NE: r = a != b; break;
          case BAND: r = toInt(a) & toInt(b); break;
          case BOR: r = toInt(a) | toInt(b); break;
          case BXOR: r = toInt(a) ^ toInt(b); break;
          case SHL: r = (int32_t)((uint32_t)toInt(a) << (toInt(b) & 31)); break;
          case SHR: r = toInt(a) >> (toInt(b) & 31); break;
          case AND: r = a != 0 && b != 0; break;
          case OR: r = a != 0 || b != 0; break;
          default: r = 0;
        }
        st[sp - 1] = r;
      }
    }
  }
  return sp ? st[sp - 1] : 0;
}
