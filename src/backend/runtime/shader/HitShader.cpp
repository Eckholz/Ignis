#include "HitShader.h"
#include "Logger.h"
#include "loader/Loader.h"
#include "loader/LoaderBSDF.h"
#include "loader/LoaderLight.h"
#include "loader/LoaderTechnique.h"
#include "loader/ShaderUtils.h"
#include "loader/ShadingTree.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string HitShader::setup(int entity_id, LoaderContext& ctx)
{
    std::stringstream stream;

    stream << LoaderTechnique::generateHeader(ctx) << std::endl;

    stream << "#[export] fn ig_hit_shader(settings: &Settings, entity_id: i32, first: i32, last: i32) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
           << std::endl;

    stream << ShaderUtils::generateDatabase() << std::endl;

    ShadingTree tree(ctx);
    const bool requireLights = ctx.CurrentTechniqueVariantInfo().UsesLights;
    if (requireLights) {
        stream << LoaderLight::generate(tree, false) << std::endl;
    }

    stream << "  let acc  = SceneAccessor {" << std::endl
           << "    info     = " << ShaderUtils::inlineSceneInfo(ctx) << "," << std::endl
           << "    shapes   = shapes," << std::endl
           << "    entities = entities," << std::endl
           << "  };" << std::endl
           << std::endl;

    stream << "  let scene = Scene {" << std::endl
           << "    info     = acc.info," << std::endl
           << "    database = acc" << std::endl
           << "  };" << std::endl
           << std::endl;

    const std::string bsdf_name = ctx.Environment.Entities[entity_id].BSDF;
    stream << LoaderBSDF::generate(bsdf_name, tree);

    const std::string entity_name = ctx.Environment.Entities[entity_id].Name;
    const bool isLight            = ctx.Environment.AreaLightsMap.count(entity_name) > 0;

    if (isLight && requireLights) {
        const std::string light_name = ctx.Environment.AreaLightsMap[entity_name];
        stream << "  let shader : Shader = @|ray, hit, surf| make_emissive_material(surf, bsdf_" << ShaderUtils::escapeIdentifier(bsdf_name) << "(ray, hit, surf), "
               << "light_" << ShaderUtils::escapeIdentifier(light_name) << ");" << std::endl
               << std::endl;
    } else {
        stream << "  let shader : Shader = @|ray, hit, surf| make_material(bsdf_" << ShaderUtils::escapeIdentifier(bsdf_name) << "(ray, hit, surf));" << std::endl
               << std::endl;
    }

    stream << "  let spp = " << ctx.SamplesPerIteration << " : i32;" << std::endl;

    // Will define technique
    stream << LoaderTechnique::generate(ctx) << std::endl
           << std::endl;

    stream << "  let use_framebuffer = " << (!ctx.CurrentTechniqueVariantInfo().LockFramebuffer ? "true" : "false") << ";" << std::endl
           << "  device.handle_hit_shader(entity_id, shader, scene, technique, first, last, spp, use_framebuffer);" << std::endl
           << "}" << std::endl;

    return stream.str();
}

} // namespace IG