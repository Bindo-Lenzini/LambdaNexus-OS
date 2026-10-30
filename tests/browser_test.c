#define main browser_program_main
#define puts user_puts
#define printf user_printf
#include "../user/browser.c"
#undef printf
#undef puts
#undef main

int main(void) {
    const char input[] =
        "<h1>Lambda &amp; Nexus</h1>"
        "<p>A basic <b>browser</b> test.</p>"
        "<script>ignored_script()</script>"
        "<style>.hidden{display:none}</style>"
        "<div>Second&nbsp;line &lt;safe&gt;</div>"
        "<p>Привет &amp; мир</p>";
    const char expected[] =
        "Lambda & Nexus\nA basic browser test.\nSecond line <safe>\nПривет & мир";
    char output[256];
    int length = html_to_text(input, (int)strlen(input), output,
                              (int)sizeof(output));

    if (length != (int)strlen(expected) || strcmp(output, expected) != 0) {
        return 1;
    }

    return 0;
}
