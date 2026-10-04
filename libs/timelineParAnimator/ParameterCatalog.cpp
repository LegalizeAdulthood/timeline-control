// Copyright (c) 2026 Richard Thomson

#include <ParameterCatalog.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace timeline_par_animator
{
namespace
{

using Json = nlohmann::json;

void require(bool valid, const std::string &context, std::string_view requirement)
{
    if (!valid)
    {
        throw std::invalid_argument("Invalid parameter catalog " + context + ": " + std::string(requirement));
    }
}

void require_object(const Json &value, const std::string &context)
{
    require(value.is_object(), context, "requires an object");
}

std::string required_string(const Json &value, std::string_view field, const std::string &context)
{
    require(value.contains(field) && value.at(field).is_string(), context, std::string(field) + " requires a string");
    return value.at(field).get<std::string>();
}

void require_choice(const std::string &value, std::initializer_list<std::string_view> choices,
    const std::string &context, std::string_view field)
{
    require(std::find(choices.begin(), choices.end(), value) != choices.end(), context,
        "unknown " + std::string(field) + " '" + value + "'");
}

void optional_choice(const Json &value, std::string_view field, std::initializer_list<std::string_view> choices,
    const std::string &context)
{
    if (value.contains(field))
    {
        require_choice(required_string(value, field, context), choices, context, field);
    }
}

void require_integer(const Json &value, const std::string &context)
{
    require(value.is_number_integer() && value >= std::numeric_limits<int>::min() &&
            value <= std::numeric_limits<int>::max(),
        context, "requires an integer in int range");
}

void metadata_fields(const Json &value, const std::string &context, std::string_view type)
{
    require_object(value, context);
    require(!required_string(value, "description", context).empty(), context, "description must not be empty");
    optional_choice(value, "format", {"raw", "slash", "slash-list", "slash-pair"}, context);
    optional_choice(value, "default-curve", {"linear", "hold", "step", "geometric"}, context);
    optional_choice(value, "extrapolate", {"clamp", "base", "omit", "cycle", "ping-pong"}, context);
    for (const std::string_view field : {"min", "max"})
    {
        if (value.contains(field))
        {
            require(value.at(field).is_number(), context, std::string(field) + " requires a number");
        }
    }
    if (value.contains("normalize"))
    {
        require(value.at("normalize").is_boolean(), context, "normalize requires a boolean");
    }
    if (value.contains("arity"))
    {
        require_integer(value.at("arity"), context + ".arity");
        const int arity = value.at("arity").get<int>();
        require(arity > 0, context, "arity must be positive");
        const int alias = type == "point2" || type == "vector2" ? 2 : type == "point3" || type == "vector3" ? 3 : 0;
        require(alias == 0 || arity == alias, context, "arity does not match type");
    }
}

void require_id_functions(const Json &value, const std::string &context)
{
    require(required_string(value, "values", context) == "id-functions", context, "values must be id-functions");
}

void parameter_metadata(const Json &value, const std::string &context)
{
    require_object(value, context);
    const std::string type = required_string(value, "type", context);
    require_choice(type,
        {"center-mag", "color-map", "corners", "complex", "double", "enum", "function-list", "yes-no", "inside",
            "integer", "integer-or-enum", "integer-tuple", "miim", "numeric-tuple", "numeric-tuple-or-enum", "outside",
            "point2", "point3", "potential", "string", "vector2", "vector3"},
        context, "type");
    metadata_fields(value, context, type);
    if (type == "function-list")
    {
        require_id_functions(value, context);
        return;
    }
    const bool discrete = type == "enum" || type == "inside" || type == "outside" || type == "integer-or-enum" ||
        type == "numeric-tuple-or-enum";
    if (value.contains("values"))
    {
        const Json &values = value.at("values");
        require(values.is_array(), context, "values requires an array");
        for (const Json &entry : values)
        {
            require(entry.is_string(), context, "values contains a non-string");
        }
        require(discrete ? !values.empty() : values.empty(), context,
            discrete ? "missing discrete values" : "values require a discrete type");
    }
    else
    {
        require(!discrete, context, "missing discrete values");
    }
}

void function_metadata(const Json &value, const std::string &name, const std::string &context)
{
    require_object(value, context);
    require(name.size() == 3 && name[0] == 'f' && name[1] == 'n' && name[2] >= '1' && name[2] <= '4', context,
        "invalid function key");
    require(required_string(value, "type", context) == "enum", context, "function type must be enum");
    require_id_functions(value, context);
    metadata_fields(value, context, "enum");
}

void validate_functions(const Json &functions, const std::string &context)
{
    require_object(functions, context);
    for (const auto &[name, value] : functions.items())
    {
        function_metadata(value, name, context + "." + name);
    }
}

void validate_slots(const Json &value, const std::string &context)
{
    require(value.contains("slots") && value.at("slots").is_array(), context, "slots requires an array");
    for (const Json &slot : value.at("slots"))
    {
        require_integer(slot, context + ".slots");
    }
}

void validate_fractal_params(const Json &params, const std::string &context)
{
    require_object(params, context);
    if (params.contains("slots"))
    {
        require(params.at("slots").is_array(), context, "slots requires an array");
        for (const Json &slot : params.at("slots"))
        {
            parameter_metadata(slot, context + ".slots");
            require(slot.contains("index"), context, "slot requires index");
            require_integer(slot.at("index"), context + ".index");
            required_string(slot, "name", context);
        }
    }
    if (params.contains("groups"))
    {
        const Json &groups = params.at("groups");
        require_object(groups, context + ".groups");
        for (const auto &[name, value] : groups.items())
        {
            parameter_metadata(value, context + "." + name);
            validate_slots(value, context + "." + name);
        }
    }
}

void validate_formula_params(const Json &params, const std::string &context)
{
    require_object(params, context);
    if (!params.contains("knobs"))
    {
        return;
    }
    require_object(params.at("knobs"), context + ".knobs");
    for (const auto &[name, value] : params.at("knobs").items())
    {
        const std::string knob = context + "." + name;
        require_object(value, knob);
        const std::string type = required_string(value, "type", knob);
        require_choice(type, {"integer", "real", "complex"}, knob, "knob type");
        metadata_fields(value, knob, type);
        const std::string variable = required_string(value, "variable", knob);
        require(variable.size() >= 2 && variable[0] == 'p' && variable[1] >= '1' && variable[1] <= '4', knob,
            "invalid variable");
        require(
            type == "complex" ? variable.size() == 2 : variable.substr(2) == ".real" || variable.substr(2) == ".imag",
            knob, "invalid variable component");
    }
}

void validate_catalog(const Json &catalog)
{
    require_object(catalog, "root");
    require(catalog.contains("parameters"), "root", "requires parameters");
    require_object(catalog.at("parameters"), "parameters");
    for (const auto &[name, value] : catalog.at("parameters").items())
    {
        parameter_metadata(value, "parameters." + name);
    }
    for (const std::string section : {"fractal-types", "formula-entries"})
    {
        if (!catalog.contains(section))
        {
            continue;
        }
        require_object(catalog.at(section), section);
        for (const auto &[name, value] : catalog.at(section).items())
        {
            const std::string context = section + "." + name;
            require_object(value, context);
            if (value.contains("params"))
            {
                if (section == "fractal-types")
                {
                    validate_fractal_params(value.at("params"), context + ".params");
                }
                else
                {
                    validate_formula_params(value.at("params"), context + ".params");
                }
            }
            if (value.contains("functions"))
            {
                validate_functions(value.at("functions"), context + ".functions");
            }
        }
    }
}

} // namespace

void append_parameter_catalog(Json &target, const Json &catalog)
{
    validate_catalog(catalog);
    for (const std::string section : {"parameters", "fractal-types", "formula-entries"})
    {
        if (!catalog.contains(section))
        {
            continue;
        }
        for (const auto &[name, value] : catalog.at(section).items())
        {
            if (target.at(section).contains(name))
            {
                throw std::invalid_argument("Duplicate " + section + " metadata '" + name + "' in parameter catalogs");
            }
            target.at(section)[name] = value;
        }
    }
}

} // namespace timeline_par_animator
