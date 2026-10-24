
#define _POSIX_C_SOURCE 200809L

#include "lexer.h"
#include "parser.h"
#include "resolver.h"
#include "interpreter.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define REPORT_TITLE "LOX INTERPRETER - TEST REPORT"

static bool has_lox_extension(const char *path)
{
    size_t length;

    if (path == NULL) {
        return false;
    }

    length = strlen(path);
    return length >= 4 &&
           strcmp(path + length - 4, ".lox") == 0;
}

static char *default_report_path(const char *input_path)
{
    size_t length;
    size_t base_length;
    char *result;

    if (input_path == NULL) {
        return NULL;
    }

    length = strlen(input_path);
    base_length = has_lox_extension(input_path)
                      ? length - 4
                      : length;

    /* Can them 4 ky tu ".txt" va 1 ky tu '\0'. */
    if (base_length > (size_t)-1 - 5) {
        return NULL;
    }

    result = (char *)malloc(base_length + 5);
    if (result == NULL) {
        return NULL;
    }

    memcpy(result, input_path, base_length);
    memcpy(result + base_length, ".txt", 5);

    return result;
}

static bool reset_temp_file(FILE *file)
{
    if (file == NULL || fflush(file) != 0) {
        return false;
    }

    if (ftruncate(fileno(file), 0) != 0) {
        return false;
    }

    return fseek(file, 0, SEEK_SET) == 0;
}

static bool redirect_stderr(FILE *capture, int *saved_stderr)
{
    int saved;

    if (capture == NULL ||
        saved_stderr == NULL ||
        fflush(stderr) != 0) {
        return false;
    }

    saved = dup(STDERR_FILENO);
    if (saved < 0) {
        return false;
    }

    if (dup2(fileno(capture), STDERR_FILENO) < 0) {
        close(saved);
        return false;
    }

    clearerr(stderr);
    *saved_stderr = saved;

    return true;
}

static bool restore_stderr(int saved_stderr)
{
    bool success = true;

    if (fflush(stderr) != 0) {
        success = false;
    }

    if (dup2(saved_stderr, STDERR_FILENO) < 0) {
        success = false;
    }

    if (close(saved_stderr) != 0) {
        success = false;
    }

    clearerr(stderr);
    return success;
}

/* Sao chep noi dung cua mot luong sang luong khac. */
static bool copy_stream(FILE *source, FILE *destination)
{
    int ch;

    if (source == NULL || destination == NULL) {
        return false;
    }

    if (fflush(source) != 0 ||
        fseek(source, 0, SEEK_SET) != 0) {
        return false;
    }

    while ((ch = fgetc(source)) != EOF) {
        if (fputc(ch, destination) == EOF) {
            return false;
        }
    }

    if (ferror(source) || fflush(destination) != 0) {
        return false;
    }

    clearerr(source);

    return fseek(source, 0, SEEK_SET) == 0;
}

/* Ghi mot phan bao cao dang van ban thuần. */
static bool write_temp_section(
    FILE *report,
    const char *heading,
    const char *empty_message,
    FILE *contents)
{
    int ch;
    bool has_content = false;
    int last_char = '\n';

    if (report == NULL ||
        heading == NULL ||
        empty_message == NULL ||
        contents == NULL) {
        return false;
    }

    if (fflush(contents) != 0 ||
        fseek(contents, 0, SEEK_SET) != 0) {
        return false;
    }

    if (fprintf(report, "\n========================================\n"
                        "%s\n"
                        "========================================\n",
                heading) < 0) {
        return false;
    }

    while ((ch = fgetc(contents)) != EOF) {
        has_content = true;
        last_char = ch;

        if (fputc(ch, report) == EOF) {
            return false;
        }
    }

    if (ferror(contents)) {
        return false;
    }

    if (!has_content) {
        if (fprintf(report, "%s", empty_message) < 0) {
            return false;
        }
        last_char = empty_message[0] == '\0'
                        ? '\n'
                        : empty_message[strlen(empty_message) - 1];
    }

    if (last_char != '\n' && fputc('\n', report) == EOF) {
        return false;
    }

    return fseek(contents, 0, SEEK_SET) == 0;
}

static bool write_report_header(
    FILE *report,
    const char *input_path,
    const char *build_status)
{
    if (fprintf(report,
                "%s\n\n"
                "Input file: %s\n"
                "Components: Lexer, Parser, Resolver, Interpreter (C)\n",
                REPORT_TITLE,
                input_path) < 0) {
        return false;
    }

    if (build_status != NULL &&
        strcmp(build_status, "SUCCESS") == 0) {
        return fprintf(report,
                       "C compilation: SUCCESS\n") >= 0;
    }

    return fprintf(report,
                   "C compilation: NOT VERIFIED\n") >= 0;
}

static bool report_status(
    FILE *report,
    const char *phase,
    const char *status,
    const char *detail)
{
    if (fprintf(report, "%s: %s", phase, status) < 0) {
        return false;
    }

    if (detail != NULL && detail[0] != '\0') {
        if (fprintf(report, " - %s", detail) < 0) {
            return false;
        }
    }

    return fputc('\n', report) != EOF;
}

static bool report_lexical_errors(
    FILE *report,
    const TokenArray *tokens,
    size_t *error_count)
{
    size_t i;

    if (report == NULL ||
        tokens == NULL ||
        error_count == NULL) {
        return false;
    }

    *error_count = 0;

    if (fprintf(report, "\nLEXICAL ERRORS\n") < 0) {
        return false;
    }

    for (i = 0; i < tokens->count; ++i) {
        const Token *token = &tokens->items[i];

        if (token->type != TOKEN_ERROR) {
            continue;
        }

        if (fprintf(report,
                    "%zu. Line %d, column %d: %s\n",
                    *error_count + 1,
                    token->line,
                    token->column,
                    token->lexeme != NULL
                        ? token->lexeme
                        : "(no error message)") < 0) {
            return false;
        }

        ++(*error_count);
    }

    if (*error_count == 0) {
        return fprintf(report, "No lexical errors found.\n") >= 0;
    }

    return true;
}

static bool write_final_status(
    FILE *report,
    bool success,
    const char *reason)
{
    if (fprintf(report,
                "\n========================================\n"
                "FINAL RESULT\n"
                "========================================\n") < 0) {
        return false;
    }

    if (success) {
        return fprintf(
                   report,
                   "SUCCESS: Lexer, Parser, Resolver and Runtime completed without errors.\n"
               ) >= 0;
    }

    if (fprintf(report, "FAILED: %s\n",
                reason != NULL
                    ? reason
                    : "An error occurred during testing.") < 0) {
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    const char *input_path = NULL;
    const char *build_status = NULL;
    const char *report_path = NULL;

    char *owned_report_path = NULL;
    char *source = NULL;

    FILE *report = NULL;
    FILE *diagnostics = NULL;
    FILE *program_output = NULL;

    TokenArray tokens = {0};
    StmtList statements = {0};

    Parser parser;
    Interpreter interpreter;
    Resolver resolver;

    bool interpreter_initialized = false;
    bool resolver_initialized = false;
    bool stderr_redirected = false;
    bool terminal_result_printed = false;

    bool parse_ok = false;
    bool resolver_ok = false;
    bool runtime_ok = false;
    bool overall_success = false;

    bool report_ok = true;

    size_t lexical_error_count = 0;

    int exit_code = 1;
    int saved_stderr = -1;

    const char *failure_reason =
        "Testing failed. Check the report for details.";

    if (argc < 2 || argc > 3) {
        fprintf(stderr,
                "Usage: %s <input.lox> [report.txt]\n",
                argv[0]);
        return 2;
    }

    /*
     * FIX 1:
     * Phai gan input_path truoc khi in hoac su dung.
     */
    input_path = argv[1];
    build_status = getenv("LOX_C_BUILD_STATUS");

    if (argc == 3) {
        report_path = argv[2];
    } else {
        owned_report_path = default_report_path(input_path);

        if (owned_report_path == NULL) {
            fprintf(stderr,
                    "[FAILED] Cannot allocate report path.\n");
            return 2;
        }

        report_path = owned_report_path;
    }

    printf("[RUN] %s\n", input_path);
    fflush(stdout);

    report = fopen(report_path, "w");

    if (report == NULL) {
        fprintf(stderr,
                "[FAILED] Cannot create report '%s': %s\n",
                report_path,
                strerror(errno));

        free(owned_report_path);
        return 2;
    }

    diagnostics = tmpfile();
    program_output = tmpfile();

    if (diagnostics == NULL || program_output == NULL) {
        failure_reason =
            "Cannot create temporary files for output and diagnostics.";
        goto cleanup;
    }

    if (!write_report_header(report, input_path, build_status)) {
        failure_reason = "Cannot write report header.";
        report_ok = false;
        goto cleanup;
    }

    if (fprintf(report, "\nTEST STAGES\n") < 0) {
        failure_reason = "Cannot write test stages.";
        report_ok = false;
        goto cleanup;
    }

    if (!has_lox_extension(input_path)) {
        failure_reason = "Input file must have the .lox extension.";

        (void)report_status(
            report, "Input", "FAILED",
            "Expected a file ending in .lox.");

        goto cleanup;
    }

    /* DOC FILE NGUON */
    if (!redirect_stderr(diagnostics, &saved_stderr)) {
        failure_reason = "Cannot capture source-file diagnostics.";
        goto cleanup;
    }

    stderr_redirected = true;

    if (!readSourceFile(input_path, &source)) {
        (void)restore_stderr(saved_stderr);
        stderr_redirected = false;

        failure_reason = "Cannot read the input file.";

        (void)report_status(
            report, "Input", "FAILED",
            "Unable to read source file.");

        goto cleanup;
    }

    if (!restore_stderr(saved_stderr)) {
        stderr_redirected = false;
        failure_reason = "Cannot restore stderr after reading input.";
        goto cleanup;
    }

    stderr_redirected = false;

    (void)report_status(report, "Input", "SUCCESS", NULL);

    /* LEXER */
    initTokenArray(&tokens);

    {
        Lexer lexer;

        initLexer(&lexer, source);

        if (!scanTokens(&lexer, &tokens)) {
            failure_reason =
                "Lexer failed, possibly due to memory allocation.";

            (void)report_status(
                report, "Lexer", "INTERNAL ERROR",
                "Token scanning failed.");

            goto cleanup;
        }
    }

    if (!report_lexical_errors(
            report, &tokens, &lexical_error_count)) {
        failure_reason = "Cannot write lexical error report.";
        report_ok = false;
        goto cleanup;
    }

    if (lexical_error_count != 0) {
        failure_reason = "Source contains lexical errors.";

        (void)report_status(
            report, "Lexer", "FAILED",
            "Found invalid tokens.");

        goto cleanup;
    }

    (void)report_status(report, "Lexer", "SUCCESS", NULL);

    /* PARSER */
    parser_init(&parser, tokens.items, tokens.count);

    if (!reset_temp_file(diagnostics) ||
        !redirect_stderr(diagnostics, &saved_stderr)) {
        failure_reason = "Cannot initialize parser diagnostics.";
        goto cleanup;
    }

    stderr_redirected = true;

    parse_ok = parser_parse(&parser, &statements);

    if (!restore_stderr(saved_stderr)) {
        stderr_redirected = false;
        failure_reason = "Cannot restore stderr after parsing.";
        goto cleanup;
    }

    stderr_redirected = false;

    if (!parse_ok) {
        failure_reason = parser.out_of_memory
                             ? "Parser ran out of memory."
                             : "Source contains syntax errors.";

        (void)report_status(
            report,
            "Parser",
            parser.out_of_memory ? "OUT OF MEMORY" : "FAILED",
            parser.out_of_memory
                ? "Unable to allocate AST."
                : "Syntax errors were detected.");

        goto cleanup;
    }

    (void)report_status(report, "Parser", "SUCCESS", NULL);

    /* INTERPRETER INITIALIZATION */
    if (!interpreter_init(
            &interpreter, program_output, diagnostics)) {
        failure_reason = "Cannot initialize the interpreter.";
        goto cleanup;
    }

    interpreter_initialized = true;

    /* RESOLVER */
    resolver_init(&resolver, &interpreter);
    resolver_initialized = true;

    resolver_set_error_stream(&resolver, diagnostics);

    resolver_set_binding_sink(
        &resolver,
        interpreter_resolver_binding_sink,
        &interpreter);

    /*
     * Xoa cac chan doan cu truoc khi Resolver chay.
     */
    if (!reset_temp_file(diagnostics)) {
        failure_reason = "Cannot reset resolver diagnostics.";
        goto cleanup;
    }

    resolver_resolve_statements(
        &resolver,
        statements.items,
        statements.count);

    resolver_ok =
        !resolver_had_error(&resolver) &&
        !resolver_out_of_memory(&resolver);

    if (!resolver_ok) {
        failure_reason = resolver_out_of_memory(&resolver)
                             ? "Resolver ran out of memory."
                             : "Resolver detected semantic errors.";

        (void)report_status(
            report,
            "Resolver",
            resolver_out_of_memory(&resolver)
                ? "OUT OF MEMORY"
                : "FAILED",
            resolver_out_of_memory(&resolver)
                ? "Unable to manage scopes or bindings."
                : "Semantic errors were detected.");

        goto cleanup;
    }

    (void)report_status(report, "Resolver", "SUCCESS", NULL);

    /* RUNTIME */
    runtime_ok = interpreter_interpret_list(
        &interpreter, &statements);

    /*
     * In dau ra thuc te cua chuong trinh Lox.
     * copy_stream dua con tro file ve dau sau khi sao chep.
     */
    if (!copy_stream(program_output, stdout)) {
        runtime_ok = false;
        failure_reason = "Cannot display program output.";
    }

    if (runtime_ok) {
        (void)report_status(report, "Runtime", "SUCCESS", NULL);
    } else {
        failure_reason = "Runtime execution failed.";

        (void)report_status(
            report,
            "Runtime",
            "FAILED",
            "See diagnostic messages below.");
    }

    terminal_result_printed = true;

    if (runtime_ok) {
        printf("[OK] Execution completed successfully.\n");
    } else {
        printf("[FAILED] Runtime error.\n");

        if (diagnostics != NULL) {
            (void)copy_stream(diagnostics, stderr);
        }
    }

    overall_success =
        parse_ok && resolver_ok && runtime_ok;

    exit_code = overall_success ? 0 : 1;

cleanup:
    /*
     * Neu loi xay ra truoc Runtime, van phai thong bao
     * that bai tren terminal.
     */
    if (!terminal_result_printed) {
        printf("[FAILED] %s\n", failure_reason);

        if (diagnostics != NULL) {
            (void)copy_stream(diagnostics, stderr);
        }

        /*
         * Hien thi loi Lexer ro rang tren terminal.
         */
        if (lexical_error_count > 0) {
            size_t i;

            for (i = 0; i < tokens.count; ++i) {
                const Token *token = &tokens.items[i];

                if (token->type == TOKEN_ERROR) {
                    fprintf(
                        stderr,
                        "[LEXER] Line %d, column %d: %s\n",
                        token->line,
                        token->column,
                        token->lexeme != NULL
                            ? token->lexeme
                            : "Invalid token");
                }
            }
        }
    }

    if (stderr_redirected) {
        (void)restore_stderr(saved_stderr);
        stderr_redirected = false;
    }

    if (report != NULL) {
        /*
         * Ghi dau ra va chan doan vao bao cao.
         */
        if (program_output != NULL &&
            !write_temp_section(
                report,
                "PROGRAM OUTPUT",
                "Program produced no output.",
                program_output)) {
            report_ok = false;
        }

        if (diagnostics != NULL &&
            !write_temp_section(
                report,
                "DIAGNOSTICS",
                "No diagnostic messages.",
                diagnostics)) {
            report_ok = false;
        }

        if (!write_final_status(
                report,
                overall_success && report_ok,
                failure_reason)) {
            report_ok = false;
        }

        if (fflush(report) != 0) {
            report_ok = false;
        }

        if (fclose(report) != 0) {
            report_ok = false;
        }

        report = NULL;
    }

    if (diagnostics != NULL) {
        fclose(diagnostics);
        diagnostics = NULL;
    }

    if (program_output != NULL) {
        fclose(program_output);
        program_output = NULL;
    }

    if (resolver_initialized) {
        resolver_free(&resolver);
    }

    if (interpreter_initialized) {
        interpreter_free(&interpreter);
    }

    stmt_list_free(&statements);
    freeTokens(&tokens);
    free(source);
    free(owned_report_path);

    if (!report_ok) {
        fprintf(stderr,
                "[FAILED] Could not write the complete report.\n");
        exit_code = 1;
    }

    if (report_path != NULL) {
        printf("[REPORT] %s\n", report_path);
    }

    if (exit_code == 0) {
        printf("[RESULT] PASS\n");
    } else {
        printf("[RESULT] FAIL\n");
    }

    return exit_code;
}
