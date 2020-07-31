//
// TM & (c) 2020 Lucasfilm Entertainment Company Ltd. and Lucasfilm Ltd.
// All rights reserved.  See LICENSE.txt for license.
//

#include <MaterialXGenGlsl/GlslResourceBindingContext.h>

namespace MaterialX
{

//
// GlslResourceBindingContext
//
GlslResourceBindingContext::GlslResourceBindingContext()
{
    _requiredExtensions.insert("GL_ARB_shading_language_420pack");
}

void GlslResourceBindingContext::initialize()
{
    // Reset bind location counter.
    _hwBindLocation = 0;
}

void GlslResourceBindingContext::emitDirectives(GenContext& context, ShaderStage& stage)
{
    ShaderGenerator& generator = context.getShaderGenerator();

    for (auto& extension : _requiredExtensions)
    {
        generator.emitLine("#extension " + extension + " : enable", stage, false);
    }
}

void GlslResourceBindingContext::emitResourceBindings(GenContext& context, const VariableBlock& uniforms, ShaderStage& stage)
{
    ShaderGenerator& generator = context.getShaderGenerator();
    const Syntax& syntax = generator.getSyntax();

    // First, emit all value uniforms in a block with single layout binding
    bool hasValueUniforms = false;
    for (auto uniform : uniforms.getVariableOrder())
    {
        if (uniform->getType() != Type::FILENAME)
        {
            hasValueUniforms = true;
            break;
        }
    }
    if (hasValueUniforms)
    {
        generator.emitLine("layout (std140, binding=" + std::to_string(_hwBindLocation++) + ") " + 
                           syntax.getUniformQualifier() + " " + uniforms.getName() + "_" + stage.getName(), 
                           stage, false);
        generator.emitScopeBegin(stage);
        for (auto uniform : uniforms.getVariableOrder())
        {
            if (uniform->getType() != Type::FILENAME)
            {
                generator.emitLineBegin(stage);
                generator.emitVariableDeclaration(uniform, EMPTY_STRING, context, stage, false);
                generator.emitString(Syntax::SEMICOLON, stage);
                generator.emitLineEnd(stage, false);
            }
        }
        generator.emitScopeEnd(stage, true);
    }

    // Second, emit all sampler uniforms as separate uniforms with separate layout bindings
    for (auto uniform : uniforms.getVariableOrder())
    {
        if (uniform->getType() == Type::FILENAME)
        {
            generator.emitString("layout (binding=" + std::to_string(_hwBindLocation++) + ") " + syntax.getUniformQualifier() + " ", stage);
            generator.emitVariableDeclaration(uniform, EMPTY_STRING, context, stage, false);
            generator.emitLineEnd(stage, true);
        }
    }

    generator.emitLineBreak(stage);
}

void GlslResourceBindingContext::emitStructuredResourceBindings(GenContext& context,
    const VariableBlock& uniforms, ShaderStage& stage, const std::string& structInstanceName,
    const std::string& arraySuffix)
{
    ShaderGenerator& generator = context.getShaderGenerator();
    const Syntax& syntax = generator.getSyntax();

    // Glsl struct need to be aligned. We make a best effort to base align struct members and add
    // padding if required
    // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_uniform_buffer_object.txt

    const size_t baseAlignment = 16;
    std::unordered_map<const TypeDesc*, size_t> alignmentMap({ { Type::FLOAT, baseAlignment / 4 },
        { Type::INTEGER, baseAlignment / 4 }, { Type::BOOLEAN, baseAlignment / 4 },
        { Type::COLOR2, baseAlignment }, { Type::COLOR3, baseAlignment },
        { Type::COLOR4, baseAlignment }, { Type::VECTOR2, baseAlignment },
        { Type::VECTOR3, baseAlignment }, { Type::VECTOR4, baseAlignment },
        { Type::MATRIX33, baseAlignment * 4 }, { Type::MATRIX44, baseAlignment * 4 } });

    //Get struct alignment and size
    // alignment, uniform member index
    vector<std::pair<size_t, size_t>> memberOrder;
    size_t structSize = 0;
    for (size_t i = 0; i < uniforms.size(); ++i)
    {
        auto it = alignmentMap.find(uniforms[i]->getType());
        if (it == alignmentMap.end())
        {
            structSize += baseAlignment;
            memberOrder.push_back(std::make_pair(baseAlignment, i));
        }
        else
        {
            structSize += it->second;
            memberOrder.push_back(std::make_pair(it->second, i));
        }
    }

    // Align up and determine number of padding floats to add
    const size_t numPaddingfloats =
        (((structSize + (baseAlignment - 1)) & ~(baseAlignment - 1)) - structSize) / 4;

    // Sort order from largest to smallest
    std::sort(memberOrder.begin(), memberOrder.end(),
        [](const std::pair<size_t, size_t>& a, const std::pair<size_t, size_t>& b) {
            return a.first > b.first;
        });

    // Emit the struct
    generator.emitLine("struct " + uniforms.getName(), stage, false);
    generator.emitScopeBegin(stage);

    for (size_t i = 0; i < uniforms.size(); ++i)
    {
        size_t variableIndex = memberOrder[i].second;
        generator.emitLineBegin(stage);
        generator.emitVariableDeclaration(
            uniforms[variableIndex], EMPTY_STRING, context, stage, false);
        generator.emitString(Syntax::SEMICOLON, stage);
        generator.emitLineEnd(stage, false);
    }

    // Emit padding
    for (size_t i = 0; i < numPaddingfloats; ++i)
    {
        generator.emitLine("float pad" + std::to_string(i), stage, true);
    }
    generator.emitScopeEnd(stage, true);


    // emit the binding info
    generator.emitLineBreak(stage);
    generator.emitLine("layout (std140, binding=" + std::to_string(_hwBindLocation++) +
        ") " + syntax.getUniformQualifier() + " " + uniforms.getName() + "_" +
        stage.getName(),
    stage, false);
    generator.emitScopeBegin(stage);
    generator.emitLine(uniforms.getName() + " " + structInstanceName + arraySuffix, stage);
    generator.emitScopeEnd(stage, true);
}
}
