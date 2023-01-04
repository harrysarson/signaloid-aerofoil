#include <math.h>
#include <stdio.h>

#ifndef LOCAL

#include <uncertain.h>

#endif // Local

/// Model of lift over aerofoil.
///
/// All quantities are in SI units.


typedef struct {

    /// It does not matter what this pressure is measured relative to, but all
    /// pressure measurements must have the same base.
    double pressure;

    /// Sections on the top will have an angle with magnitude in the range `[0, pi/2]`
    /// Sections on the bottom will have an angle with magnitude in the range `[pi/2, pi]`
    double angle_between_normal_and_vertical;

    double section_length;
} PreProcessedDataPoint;

typedef struct {
    /// Height difference measured by pitot static tube.
    double pitot_static_height_difference;

    /// See note in `PreProcessedDataPoint`.
    double angle_between_normal_and_vertical;

    double section_length;
} RawDataPoint;

typedef struct {

    /// Density of the fluid flowing over the wing.
    double stream_fluid_density;

    /// Density of the fluid in the pitot_static_tube.
    double pitot_static_fluid_density;

    /// Gravitational constant
    double g;

    /// Measured on far from the aerofoil, used to determine airflow speed.
    double airflow_pitot_static_height_difference;;

} ModelParameters;


typedef enum {
    PreProcessResult_Ok,
    PreProcessResult_InputNull,
    PreProcessResult_OutputTooShort,
} PreProcessResult;

static double velocity_squared_from_pitot_static(double pitot_static_height_difference, const ModelParameters *params) {
    return
        2 * params->pitot_static_fluid_density * params->g * pitot_static_height_difference
        / params->stream_fluid_density;

}

/// Produce `PreProcessedDataPoint`s sutiable for calculating aerofoil lift.
static PreProcessResult pre_process(
  const ModelParameters *params,
  const RawDataPoint *raw_data, size_t raw_data_len,
  PreProcessedDataPoint *data_out, size_t out_len
) {

    if (raw_data == NULL || params == NULL || data_out == NULL) {
        return PreProcessResult_InputNull;
    }

    if (out_len < raw_data_len) {
        return PreProcessResult_OutputTooShort;
    }

    double airflow_speed_squared = velocity_squared_from_pitot_static(params->airflow_pitot_static_height_difference, params);

    for (size_t i = 0; i < raw_data_len; ++i) {

        double v_squared_here = velocity_squared_from_pitot_static(raw_data[i].pitot_static_height_difference, params);

        // This pressure is offset by the atmospheric pressure. That is fine because atmospheric pressure
        // is constant above and below the wing so cannot contribute to lift.
        double pressure = (airflow_speed_squared - v_squared_here) * params->stream_fluid_density / 2;

        PreProcessedDataPoint ret = {
            .pressure = pressure,
            .angle_between_normal_and_vertical = raw_data->angle_between_normal_and_vertical,
            .section_length = raw_data->section_length,
        };
        data_out[i] = ret;
    }

    return PreProcessResult_Ok;
}


static double calculate_lift_per_unit_length(const PreProcessedDataPoint *data, size_t len) {

    double lift = 0;

    for (size_t i = 0; i < len; i++) {
        lift -= data[i].pressure * cos(data[i].angle_between_normal_and_vertical) * data[i].section_length;
    }

    return lift;
}

static double uncertain_with_error(double best_guess, double error) {
#ifndef LOCAL
    return libUncertainDoubleUniformDist(best_guess - error / 2, best_guess + error / 2);
#else
    return best_guess;
#endif // LOCAL
}

static double uncertain_with_fractional_error(double best_guess, double fractional_error) {
    return uncertain_with_error(best_guess, fabs(best_guess) * fractional_error);
}

int main() {
    enum size_t { n_data_points = 6 };

    double error_in_height_differences = 0.2e-3;

    // Uncertainty in angle of each section in radians
    double error_in_shape = M_PI * 2 / 5e-3;

    RawDataPoint data[n_data_points] = {
        {
            .pitot_static_height_difference = uncertain_with_error(155e-3, error_in_height_differences),
            .angle_between_normal_and_vertical = uncertain_with_error(0, error_in_shape),
            .section_length = uncertain_with_fractional_error(0.1, 0.05),
        },
        {
            .pitot_static_height_difference = uncertain_with_error(109e-3, error_in_height_differences),
            .angle_between_normal_and_vertical = uncertain_with_error(0, error_in_shape),
            .section_length = uncertain_with_fractional_error(0.1, 0.05),
        },
        {
            .pitot_static_height_difference = uncertain_with_error(124e-3, error_in_height_differences),
            .angle_between_normal_and_vertical = uncertain_with_error(0, error_in_shape),
            .section_length = uncertain_with_fractional_error(0.1, 0.05),
        },
        {
            .pitot_static_height_difference = uncertain_with_error(120e-3, error_in_height_differences),
            .angle_between_normal_and_vertical = uncertain_with_error(0, error_in_shape),
            .section_length = uncertain_with_fractional_error(0.1, 0.05),
        },
        {
            .pitot_static_height_difference = uncertain_with_error(132e-3, error_in_height_differences),
            .angle_between_normal_and_vertical = uncertain_with_error(0, error_in_shape),
            .section_length = uncertain_with_fractional_error(0.1, 0.05),
        },
        {
            .pitot_static_height_difference = uncertain_with_error(135e-3, error_in_height_differences),
            .angle_between_normal_and_vertical = uncertain_with_error(0, error_in_shape),
            .section_length = uncertain_with_fractional_error(0.1, 0.05),
        },
        // {
        //     .pitot_static_height_difference = 13,
        //     .angle_between_normal_and_vertical = -M_PI,
        //     .section_length = 1,
        // },
    };

    ModelParameters params = {
        .stream_fluid_density = uncertain_with_error(0.965, 0.00163),
        .pitot_static_fluid_density = 1000,
        .g = 0.981,
        .airflow_pitot_static_height_difference = uncertain_with_error(0.057, error_in_height_differences),
    };

    printf("x %d\n", (int)(sizeof(data)));

    PreProcessedDataPoint processed_data[n_data_points];

    {
        PreProcessResult res = pre_process(&params, data, n_data_points, processed_data, n_data_points);

        if (res != PreProcessResult_Ok) {
            fprintf(stderr, "Error preprocessing data points! Code was %d\n", (int)res);
        }
    }

    double lift = calculate_lift_per_unit_length(processed_data, n_data_points);

    printf("Lift is %f\n", lift);
}