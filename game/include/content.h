#ifndef LINE_NOISE_CONTENT_H
#define LINE_NOISE_CONTENT_H

#include <stdint.h>

typedef enum FactIcon
{
    FACT_ICON_ADDRESS = 0,
    FACT_ICON_TRAIN,
    FACT_ICON_CAB,
    FACT_ICON_MEAL,
    FACT_ICON_MEDICINE,
    FACT_ICON_ROOM,
    FACT_ICON_MONEY,
    FACT_ICON_KEY,
    FACT_ICON_TIME,
    FACT_ICON_SECRET,
    FACT_ICON_MUSIC,
    FACT_ICON_WEATHER,
    FACT_ICON_PERSON,
    FACT_ICON_WARNING,
    FACT_ICON_YES,
    FACT_ICON_NO,
    FACT_ICON_COUNT
} FactIcon;

typedef enum ConfidenceLevel
{
    CONFIDENCE_DISTORTED = 0,
    CONFIDENCE_DOUBTFUL,
    CONFIDENCE_LIKELY,
    CONFIDENCE_CERTAIN
} ConfidenceLevel;

typedef enum SensitivityLevel
{
    SENSITIVITY_PUBLIC = 0,
    SENSITIVITY_PERSONAL,
    SENSITIVITY_RESTRICTED,
    SENSITIVITY_EXPLOSIVE
} SensitivityLevel;

typedef enum VoiceColor
{
    VOICE_COLOR_CYAN = 0,
    VOICE_COLOR_CORAL,
    VOICE_COLOR_AMBER,
    VOICE_COLOR_LIME,
    VOICE_COLOR_VIOLET,
    VOICE_COLOR_BLUE
} VoiceColor;

typedef enum DestinationSlot
{
    DESTINATION_SLOT_OWEN = 0,
    DESTINATION_SLOT_NEWS,
    DESTINATION_SLOT_RAIL,
    DESTINATION_SLOT_CAL,
    DESTINATION_SLOT_COUNT
} DestinationSlot;

typedef enum ObjectiveType
{
    OBJECTIVE_KNOWLEDGE = 0,
    OBJECTIVE_WORLD_STATE,
    OBJECTIVE_PRIVACY,
    OBJECTIVE_VERIFICATION
} ObjectiveType;

enum
{
    LN_CITIZEN_COUNT = 16,
    LN_FACT_COUNT = 5,
    LN_CALL_COUNT = 4,
    LN_OBJECTIVE_COUNT = 4,
    LN_EPILOGUE_COUNT = 4,
    LN_CALL_PAYLOAD_CAPACITY = 2,
    LN_NO_ID = 255,
    LN_NO_EXPIRY = 65535
};

typedef struct CitizenSpec
{
    uint8_t id;
    uint8_t portrait_index;
    uint8_t voice_color;
    const char *name;
    const char *role;
    const char *district;
    const char *trait_a;
    const char *trait_b;
    const char *trait_c;
} CitizenSpec;

typedef struct FactSpec
{
    uint8_t id;
    uint8_t icon;
    uint8_t source_citizen_id;
    uint8_t confidence;
    uint8_t sensitivity;
    uint16_t expiry_second;
    const char *subject;
    const char *value;
    const char *display_phrase;
} FactSpec;

typedef struct CallSpec
{
    uint8_t id;
    uint8_t caller_id;
    uint8_t portrait_index;
    uint8_t destination_slot;
    uint8_t patience_seconds;
    uint8_t payload_count;
    uint8_t payload_fact_ids[LN_CALL_PAYLOAD_CAPACITY];
    uint16_t start_second;
    const char *caller_label;
    const char *destination_label;
    const char *request_phrase;
    const char *monitor_phrase;
} CallSpec;

typedef struct ObjectiveSpec
{
    uint8_t type;
    uint8_t actor_id;
    uint8_t fact_id;
    uint8_t minimum_confidence;
    uint8_t minimum_value;
    uint16_t forbidden_receiver_mask;
    const char *id;
    const char *label;
    const char *state_key;
} ObjectiveSpec;

extern const CitizenSpec LN_CITIZENS[LN_CITIZEN_COUNT];
extern const FactSpec LN_FACTS[LN_FACT_COUNT];
extern const CallSpec LN_CALLS[LN_CALL_COUNT];
extern const ObjectiveSpec LN_OBJECTIVES[LN_OBJECTIVE_COUNT];
extern const char *const LN_DESTINATION_LABELS[DESTINATION_SLOT_COUNT];

extern const char LN_EPILOGUE_COMPLETE[];
extern const char LN_EPILOGUE_NO_CABS[];
extern const char LN_EPILOGUE_PRIVACY_LEAK[];
extern const char LN_EPILOGUE_MISSED_COURIER[];
extern const char *const LN_EPILOGUES[LN_EPILOGUE_COUNT];

#endif
