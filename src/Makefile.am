bin_PROGRAMS =  cvoicecontrol microphone_config model_editor

_common_SOURCES = audio-$(backend).c mixer-$(backend).c preprocess.c realfftf.c keypressed.c

cvoicecontrol_SOURCES = $(_common_SOURCES) bb_queue.c configuration.c model.c score.c semaphore.c cvoicecontrol.c

microphone_config_SOURCES = $(_common_SOURCES) ncurses_tools.c microphone_config.c configuration.c

model_editor_SOURCES = $(_common_SOURCES) configuration.c model.c ncurses_tools.c model_editor.c

EXTRA_DIST = audio.c audio.h bb_queue.c bb_queue.h configuration.c configuration.h keypressed.c keypressed.h microphone_config.c microphone_config.h mixer.c mixer.h model.c model.h model_editor.c model_editor.h ncurses_tools.c ncurses_tools.h preprocess.c preprocess.h queue.h realfftf.c realfftf.h score.c score.h semaphore.c semaphore.h cvoicecontrol.c cvoicecontrol.h
