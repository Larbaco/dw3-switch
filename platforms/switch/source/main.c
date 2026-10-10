#include <stdio.h>
#include <switch.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    printf("dw3-switch Phase 1 skeleton\n");
    printf("Press + to exit\n");
    consoleUpdate(NULL);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) {
            break;
        }
        if (kDown != 0) {
            printf("buttons_down=0x%llx\n", (unsigned long long)kDown);
            consoleUpdate(NULL);
        }
    }

    consoleExit(NULL);
    return 0;
}
