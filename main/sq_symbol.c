// main/sq_symbol.c —— 见 sq_symbol.h。
#include "sq_symbol.h"

#include <string.h>

bool sq_symbol_valid(const char *code)
{
    if (!code || strlen(code) != SQ_CODE_LEN) return false;
    for (unsigned i = 0; i < SQ_CODE_LEN; i++) {
        if (code[i] < '0' || code[i] > '9') return false;
    }
    const char head = code[0];
    // 一期支持沪深主板/科创板/创业板；北交所（4/8）明确拒绝。
    return head == '6' || head == '9' || head == '5' ||
           head == '0' || head == '2' || head == '3';
}

bool sq_symbol_secid(const char *code, char *out, unsigned capacity)
{
    if (!sq_symbol_valid(code) || !out || capacity < SQ_SECID_MAX) return false;
    const bool sh = code[0] == '6' || code[0] == '9' || code[0] == '5';
    // 东财 secid：沪 = "1."，深 = "0."。
    out[0] = sh ? '1' : '0';
    out[1] = '.';
    memcpy(out + 2, code, SQ_CODE_LEN);
    out[2 + SQ_CODE_LEN] = '\0';
    return true;
}

const char *sq_symbol_market_name(const char *code)
{
    if (!sq_symbol_valid(code)) return "";
    const char head = code[0];
    return (head == '6' || head == '9' || head == '5') ? "沪" : "深";
}
