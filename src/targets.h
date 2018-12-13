#pragma once

#include "fields.h"
#include "operators.h"

#define NUM_TARGET_TYPE 2
#define NUM_TARGET 29

#define new_target_list(...)                                                   \
    _new_target_list (                                                         \
        sizeof ((TARGET[]){__VA_ARGS__}) / sizeof (TARGET),                    \
        (TARGET[]){__VA_ARGS__})

enum TARGET_TYPE { TARGET_TYPE_FIELD, TARGET_TYPE_OPERATOR };

enum TARGET {
    TARGET_FIELD_DIABATIC_HEATING,
    TARGET_FIELD_VORTICITY_ADVECTION,
    TARGET_FIELD_TEMPERATURE_ADVECTION,
    TARGET_FIELD_DIABATIC_HEATING_TENDENCY,
    TARGET_FIELD_FRICTION,
    TARGET_FIELD_HORIZONTAL_WIND,
    TARGET_FIELD_OMEGA_OPERATOR,
    TARGET_FIELD_OMEGA_V,
    TARGET_FIELD_OMEGA_T,
    TARGET_FIELD_OMEGA_Q,
    TARGET_FIELD_OMEGA_F,
    TARGET_FIELD_OMEGA_A,
    TARGET_FIELD_TOTAL_OMEGA,
    TARGET_FIELD_SIGMA_PARAMETER,
    TARGET_FIELD_SURFACE_ATTENNUATION,
    TARGET_FIELD_SURFACE_PRESSURE,
    TARGET_FIELD_TEMPERATURE,
    TARGET_FIELD_TEMPERATURE_TENDENCY,
    TARGET_FIELD_VORTICITY,
    TARGET_FIELD_VORTICITY_TENDENCY,
    TARGET_FIELD_VORTICITY_TENDENCY_V,
    TARGET_FIELD_VORTICITY_TENDENCY_T,
    TARGET_FIELD_VORTICITY_TENDENCY_F,
    TARGET_FIELD_VORTICITY_TENDENCY_Q,
    TARGET_FIELD_VORTICITY_TENDENCY_A,
    TARGET_FIELD_FRICTION_U_TENDENCY,
    TARGET_FIELD_FRICTION_V_TENDENCY,
    TARGET_FIELD_STREAMFUNCTION,
    TARGET_FIELD_U_ROTATIONAL_WIND
};

typedef enum TARGET_TYPE TARGET_TYPE;
typedef enum TARGET      TARGET;
typedef struct Targets   Targets;
typedef struct Target    Target;
typedef struct TARGETS   TARGETS;

struct Target {
    TARGET_TYPE type;
    union {
        Field field;
        Operator operator;
    };
    size_t time;
};

struct Targets {
    Target target[NUM_TARGET];
};

struct TARGETS {
    TARGET this;
    TARGETS *next;
};

Targets new_targets (Options, Files, Context *);
TARGETS *push (TARGET target, TARGETS *oldhead);
TARGETS *pop (TARGETS **head);
TARGETS *_new_target_list (const size_t, const TARGET[]);
