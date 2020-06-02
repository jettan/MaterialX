//
// TM & (c) 2017 Lucasfilm Entertainment Company Ltd. and Lucasfilm Ltd.
// All rights reserved.  See LICENSE.txt for license.
//

#include <MaterialXTest/Catch/catch.hpp>
#include <MaterialXTest/MaterialXGenShader/GenShaderUtil.h>

#include <MaterialXCore/Util.h>
#include <MaterialXCore/Unit.h>

#include <MaterialXFormat/File.h>
#include <MaterialXFormat/Util.h>

#include <MaterialXGenShader/Shader.h>
#include <MaterialXGenShader/Util.h>
#include <MaterialXGenShader/TypeDesc.h>

namespace mx = MaterialX;

namespace GenShaderUtil
{
    const std::string TEST_USER_DATA_SUFFIX = "_ud";

namespace
{
    const std::string& getFileExtensionForLanguage(const std::string& language)
    {
        static const std::unordered_map<std::string, std::string> _fileExtensions = 
        {
            {"genglsl","glsl"},
            {"genosl","osl"},
            {"genmdl","mdl"}
        };
        auto it = _fileExtensions.find(language);
        return it != _fileExtensions.end() ? it->second : language;
    }
}

bool getShaderSource(mx::GenContext& context,
                    const mx::ImplementationPtr implementation,
                    mx::FilePath& sourcePath,
                    mx::FilePath& resolvedPath,
                    std::string& sourceContents)
{
    if (implementation)
    {
        sourcePath = implementation->getFile();
        resolvedPath = context.resolveSourceFile(sourcePath);
        sourceContents = mx::readFile(resolvedPath.asString());
        return !sourceContents.empty();
    }
    return false;
}

// Check that implementations exist for all nodedefs supported per generator
void checkImplementations(mx::GenContext& context,
                          const mx::StringSet& generatorSkipNodeTypes,
                          const mx::StringSet& generatorSkipNodeDefs,
                          unsigned int expectedSkipCount)
{
    mx::DocumentPtr doc = mx::createDocument();

    const mx::ShaderGenerator& shadergen = context.getShaderGenerator();

    mx::FileSearchPath searchPath; 
    searchPath.append(mx::FilePath::getCurrentPath() / mx::FilePath("libraries"));
    loadLibraries({ "stdlib", "pbrlib" }, searchPath, doc);

    std::string generatorId = shadergen.getLanguage() + "_" + shadergen.getTarget();
    std::string fileName = generatorId + "_implementation_check.txt";

    std::filebuf implDumpBuffer;
    implDumpBuffer.open(fileName, std::ios::out);
    std::ostream implDumpStream(&implDumpBuffer);

    context.registerSourceCodeSearchPath(searchPath);

    const std::string& language = shadergen.getLanguage();
    const std::string& target = shadergen.getTarget();

    // Node types to explicitly skip temporarily.
    mx::StringSet skipNodeTypes =
    {
        "ambientocclusion",
        "arrayappend",
        "displacement",
        "volume",
        "blackbody",
        "curveadjust",
        "conical_edf",
        "measured_edf",
        "absorption_vdf",
        "anisotropic_vdf",
        "thin_surface",
        "thin_film_brdf",
        "worleynoise2d",
        "worleynoise3d",
        "geompropvalue",
        "surfacematerial",
        "volumematerial"
    };
    skipNodeTypes.insert(generatorSkipNodeTypes.begin(), generatorSkipNodeTypes.end());

    // Explicit set of node defs to skip temporarily
    mx::StringSet skipNodeDefs =
    {
        "ND_add_displacementshader",
        "ND_add_volumeshader",
        "ND_add_vdf",
        "ND_multiply_displacementshaderF",
        "ND_multiply_displacementshaderV",
        "ND_multiply_volumeshaderF",
        "ND_multiply_volumeshaderC",
        "ND_multiply_vdfF",
        "ND_multiply_vdfC",
        "ND_mix_displacementshader",
        "ND_mix_volumeshader",
        "ND_mix_vdf",
        "ND_surfacematerial",
        "ND_volumematerial"
    };
    skipNodeDefs.insert(generatorSkipNodeDefs.begin(), generatorSkipNodeDefs.end());

    implDumpStream << "-----------------------------------------------------------------------" << std::endl;
    implDumpStream << "Scanning language: " << language << ". Target: " << target << std::endl;

    std::vector<mx::ImplementationPtr> impls = doc->getImplementations();
    implDumpStream << "-----------------------------------------------------------------------" << std::endl;
    implDumpStream << "Scanning implementations: " << std::to_string(impls.size()) << std::endl;
    for (const auto& impl : impls)
    {
        if (language == impl->getLanguage())
        {
            mx::NodeDefPtr nodedef = impl->getNodeDef();
            if (!nodedef)
            {
                std::string msg(impl->getName());
                const std::string& targetName = impl->getTarget();
                if (targetName.size())
                {
                    msg += ", target: " + targetName;
                }
                const std::string& nodedefName = impl->getNodeDefString();
                msg += ": Missing nodedef with name: " + nodedefName;
                implDumpStream << msg << std::endl;
            }
        }
    }

    std::string nodeDefNode;
    std::string nodeDefType;
    unsigned int count = 0;
    unsigned int missing = 0;
    unsigned int skipped = 0;
    std::string missing_str;
    std::string found_str;

    std::vector<mx::NodeDefPtr> nodedefs = doc->getNodeDefs();
    implDumpStream << "-----------------------------------------------------------------------" << std::endl;
    implDumpStream << "Scanning nodedefs: " << std::to_string(nodedefs.size()) << std::endl;

    // Scan through every nodedef defined
    for (mx::NodeDefPtr nodedef : nodedefs)
    {
        count++;

        const std::string& nodeDefName = nodedef->getName();
        const std::string& nodeName = nodedef->getNodeString();

        if (skipNodeTypes.count(nodeName))
        {
            found_str += "Temporarily skipping implementation required for nodedef: " + nodeDefName + ", Node : " + nodeName + ".\n";
            skipped++;
            continue;
        }
        if (skipNodeDefs.count(nodeDefName))
        {
            found_str += "Temporarily skipping implementation required for nodedef: " + nodeDefName + ", Node : " + nodeName + ".\n";
            skipped++;
            continue;
        }

        if (!requiresImplementation(nodedef))
        {
            found_str += "No implementation required for nodedef: " + nodeDefName + ", Node: " + nodeName + ".\n";
            continue;
        }

        mx::InterfaceElementPtr inter = nodedef->getImplementation(target, language);
        if (!inter)
        {
            missing++;
            missing_str += "Missing nodedef implementation: " + nodeDefName + ", Node: " + nodeName + ".\n";

            std::vector<mx::InterfaceElementPtr> inters = doc->getMatchingImplementations(nodeDefName);
            for (const auto& inter2 : inters)
            {
                mx::ImplementationPtr impl = inter2->asA<mx::Implementation>();
                if (impl)
                {
                    std::string msg("\t Cached Impl: ");
                    msg += impl->getName();
                    msg += ", nodedef: " + impl->getNodeDefString();
                    msg += ", target: " + impl->getTarget();
                    msg += ", language: " + impl->getLanguage();
                    missing_str += msg + ".\n";
                }
            }

            for (const auto& childImpl : impls)
            {
                if (childImpl->getNodeDefString() == nodeDefName)
                {
                    std::string msg("\t Doc Impl: ");
                    msg += childImpl->getName();
                    msg += ", nodedef: " + childImpl->getNodeDefString();
                    msg += ", target: " + childImpl->getTarget();
                    msg += ", language: " + childImpl->getLanguage();
                    missing_str += msg + ".\n";
                }
            }

        }
        else
        {
            mx::ImplementationPtr impl = inter->asA<mx::Implementation>();
            if (impl)
            {
                // Test if the generator has an interal implementation first
                if (shadergen.implementationRegistered(impl->getName()))
                {
                    found_str += "Found generator impl for nodedef: " + nodeDefName + ", Node: "
                        + nodeDefName + ". Impl: " + impl->getName() + ".\n";
                }

                // Check for an implementation explicitly stored
                else
                {
                    mx::FilePath sourcePath, resolvedPath;
                    std::string contents;
                    if (!getShaderSource(context, impl, sourcePath, resolvedPath, contents))
                    {
                        missing++;
                        missing_str += "Missing source code: " + sourcePath.asString() + " for nodedef: "
                            + nodeDefName + ". Impl: " + impl->getName() + ".\n";
                    }
                    else
                    {
                        found_str += "Found impl and src for nodedef: " + nodeDefName + ", Node: "
                            + nodeName + +". Impl: " + impl->getName() + ". Path: " + resolvedPath.asString() + ".\n";
                    }
                }
            }
            else
            {
                mx::NodeGraphPtr graph = inter->asA<mx::NodeGraph>();
                found_str += "Found NodeGraph impl for nodedef: " + nodeDefName + ", Node: "
                    + nodeName + ". Graph Impl: " + graph->getName();
                mx::InterfaceElementPtr graphNodeDefImpl = graph->getImplementation();
                if (graphNodeDefImpl)
                {
                    found_str += ". Graph Nodedef Impl: " + graphNodeDefImpl->getName();
                }
                found_str += ".\n";
            }
        }
    }

    implDumpStream << "Missing: " << missing << " implementations out of: " << count << " nodedefs. Skipped: " << skipped << std::endl;
    implDumpStream << missing_str << std::endl;
    implDumpStream << found_str << std::endl;

    // Should have 0 missing including skipped
    REQUIRE(missing == 0);
    REQUIRE(skipped == expectedSkipCount);

    implDumpBuffer.close();
}

void testUniqueNames(mx::GenContext& context, const std::string& stage)
{
    mx::DocumentPtr doc = mx::createDocument();

    mx::FileSearchPath searchPath;
    searchPath.append(mx::FilePath::getCurrentPath() / mx::FilePath("libraries"));
    loadLibraries({ "stdlib" }, searchPath, doc);

    const std::string exampleName = "unique_names";

    // Generate a shader with an internal node having the same name as the shader,
    // which will result in a name conflict between the shader output and the
    // internal node output
    const std::string shaderName = "unique_names";
    const std::string nodeName = shaderName;

    mx::NodeGraphPtr nodeGraph = doc->addNodeGraph("IMP_" + exampleName);
    mx::OutputPtr output1 = nodeGraph->addOutput("out", "color3");
    mx::NodePtr node1 = nodeGraph->addNode("noise2d", nodeName, "color3");

    output1->setConnectedNode(node1);

    const mx::ShaderGenerator& shadergen = context.getShaderGenerator();

    // Set the output to a restricted name
    const std::string& outputQualifier = shadergen.getSyntax().getOutputQualifier();
    output1->setName(outputQualifier);

    mx::GenOptions options;
    mx::ShaderPtr shader = shadergen.generate(shaderName, output1, context);
    REQUIRE(shader != nullptr);
    REQUIRE(shader->getSourceCode(stage).length() > 0);

    // Make sure the output and internal node output has their variable names set
    const mx::ShaderGraphOutputSocket* sgOutputSocket = shader->getGraph().getOutputSocket();
    REQUIRE(sgOutputSocket->getVariable() != outputQualifier);
    const mx::ShaderNode* sgNode1 = shader->getGraph().getNode(node1->getName());
    REQUIRE(sgNode1->getOutput()->getVariable() == "unique_names_out");
}


void ShaderGeneratorTester::checkImplementationUsage(const mx::StringSet& usedImpls,
                                                     const mx::GenContext& context,
                                                     std::ostream& stream)
{
    // Get list of implementations a given langauge.
    std::set<mx::ImplementationPtr> libraryImpls;
    const std::vector<mx::ElementPtr>& children = _dependLib->getChildren();
    for (const auto& child : children)
    {
        mx::ImplementationPtr impl = child->asA<mx::Implementation>();
        if (!impl)
        {
            continue;
        }

        if (impl->getLanguage() == _shaderGenerator->getLanguage())
        {
            libraryImpls.insert(impl);
        }
    }

    mx::StringSet whiteList;
    getImplementationWhiteList(whiteList);

    unsigned int implementationUseCount = 0;
    mx::StringVec skippedImplementations;
    mx::StringVec missedImplementations;
    for (const auto& libraryImpl : libraryImpls)
    {
        const std::string& implName = libraryImpl->getName();

        // Skip white-list items
        bool inWhiteList = false;
        for (const auto& w : whiteList)
        {
            if (implName.find(w) != std::string::npos)
            {
                inWhiteList = true;
                break;
            }
        }
        if (inWhiteList)
        {
            skippedImplementations.push_back(implName);
            implementationUseCount++;
            continue;
        }

        if (usedImpls.count(implName))
        {
            implementationUseCount++;
            continue;
        }

        if (context.findNodeImplementation(implName))
        {
            implementationUseCount++;
            continue;
        }
        missedImplementations.push_back(implName);
    }

    size_t libraryCount = libraryImpls.size();
    stream << "Tested: " << implementationUseCount << " out of: " << libraryCount << " library implementations." << std::endl;
    stream << "Skipped: " << skippedImplementations.size() << " implementations." << std::endl;
    if (skippedImplementations.size())
    {
        for (const auto& implName : skippedImplementations)
        {
            stream << "\t" << implName << std::endl;
        }
    }
    stream << "Untested: " << missedImplementations.size() << " implementations." << std::endl;
    if (missedImplementations.size())
    {
        for (const auto& implName : missedImplementations)
        {
            stream << "\t" << implName << std::endl;
        }
        CHECK(implementationUseCount == libraryCount);
    }
}


bool ShaderGeneratorTester::generateCode(mx::GenContext& context, const std::string& shaderName, mx::TypedElementPtr element,
                                         std::ostream& log, mx::StringVec testStages, mx::StringVec& sourceCode)
{
    mx::ShaderPtr shader = nullptr;
    try
    {
        shader = context.getShaderGenerator().generate(shaderName, element, context);
    }
    catch (mx::Exception& e)
    {
        log << ">> Code generation failure: " << e.what() << "\n";
        shader = nullptr;
    }
    CHECK(shader);
    if (!shader)
    {
        log << ">> Failed to generate shader for element: " << element->getNamePath() << std::endl;
        return false;
    }
    
    bool stageFailed = false;
    for (const auto& stage : testStages)
    {
        const std::string& code = shader->getSourceCode(stage);
        sourceCode.push_back(code);
        bool noSource = code.empty();
        CHECK(!noSource);
        if (noSource)
        {
            log << ">> Failed to generate source code for stage: " << stage << std::endl;
            stageFailed = true;
        }
    }
    return !stageFailed;
}

void ShaderGeneratorTester::addColorManagement()
{
    if (!_colorManagementSystem && _shaderGenerator)
    {
        const std::string language = _shaderGenerator->getLanguage();
        _colorManagementSystem = mx::DefaultColorManagementSystem::create(language);
        if (!_colorManagementSystem)
        {
            _logFile << ">> Failed to create color management system for language: " << language << std::endl;
        }
        else
        {
            _shaderGenerator->setColorManagementSystem(_colorManagementSystem);
            _colorManagementSystem->loadLibrary(_dependLib);
        }
    }
}

void ShaderGeneratorTester::addUnitSystem()
{
    if (!_unitSystem && _shaderGenerator)
    {
        const std::string language = _shaderGenerator->getLanguage();
        _unitSystem = mx::UnitSystem::create(language);
        if (!_unitSystem)
        {
            _logFile << ">> Failed to create unit system for language: " << language << std::endl;
        }
        else
        {
            _shaderGenerator->setUnitSystem(_unitSystem);
            _unitSystem->loadLibrary(_dependLib);
            _unitSystem->setUnitConverterRegistry(mx::UnitConverterRegistry::create());
            mx::UnitTypeDefPtr distanceTypeDef = _dependLib->getUnitTypeDef("distance");
            _unitSystem->getUnitConverterRegistry()->addUnitConverter(distanceTypeDef, mx::LinearUnitConverter::create(distanceTypeDef));
            _defaultDistanceUnit = "meter";            
            mx::UnitTypeDefPtr angleTypeDef = _dependLib->getUnitTypeDef("angle");
            _unitSystem->getUnitConverterRegistry()->addUnitConverter(angleTypeDef, mx::LinearUnitConverter::create(angleTypeDef));
        }
    }
}

void ShaderGeneratorTester::setupDependentLibraries()
{
    _dependLib = mx::createDocument();

    // Load the standard libraries.
    const mx::FilePathVec libraries = { "stdlib", "pbrlib", "lights" };

    loadLibraries(libraries, _libSearchPath, _dependLib, &_skipLibraryFiles);

    // Load shader definitions used in the test suite.
    loadLibrary(mx::FilePath("bxdf/standard_surface.mtlx"), _dependLib, &_libSearchPath);
    loadLibrary(mx::FilePath("bxdf/usd_preview_surface.mtlx"), _dependLib, &_libSearchPath);
}

void ShaderGeneratorTester::addSkipFiles()
{
    _skipFiles.insert("_options.mtlx");
    _skipFiles.insert("light_rig_test_1.mtlx");
    _skipFiles.insert("light_rig_test_2.mtlx");
    _skipFiles.insert("light_compound_test.mtlx");
}

void ShaderGeneratorTester::addSkipNodeDefs()
{
}

void ShaderGeneratorTester::addSkipLibraryFiles()
{
}

LightIdMap ShaderGeneratorTester::computeLightIdMap(const std::vector<mx::NodePtr>& nodes)
{
    std::unordered_map<std::string, unsigned int> idMap;
    unsigned int id = 1;
    for (const auto& node : nodes)
    {
        auto nodedef = node->getNodeDef();
        if (nodedef)
        {
            const std::string& name = nodedef->getName();
            if (!idMap.count(name))
            {
                idMap[name] = id++;
            }
        }
    }
    return idMap;
}

void ShaderGeneratorTester::findLights(mx::DocumentPtr doc, std::vector<mx::NodePtr>& lights)
{
    lights.clear();
    for (mx::NodePtr node : doc->getNodes())
    {
        const mx::TypeDesc* type = mx::TypeDesc::get(node->getType());
        if (type == mx::Type::LIGHTSHADER)
        {
            lights.push_back(node);
        }
    }
}

void ShaderGeneratorTester::registerLights(mx::DocumentPtr doc, const std::vector<mx::NodePtr>& lights,
                                           mx::GenContext& context)
{
    // Clear context light user data which is set when bindLightShader() 
    // is called. This is necessary in case the light types have already been
    // registered.
    mx::HwShaderGenerator::unbindLightShaders(context);

    if (!lights.empty())
    {
        // Create a list of unique nodedefs and ids for them
        _lightIdentifierMap = computeLightIdMap(lights);
        for (const auto& id : _lightIdentifierMap)
        {
            mx::NodeDefPtr nodedef = doc->getNodeDef(id.first);
            if (nodedef)
            {
                mx::HwShaderGenerator::bindLightShader(*nodedef, id.second, context);
            }
        }
    }

    // Clamp the number of light sources to the number registered
    unsigned int lightSourceCount = static_cast<unsigned int>(lights.size());
    context.getOptions().hwMaxActiveLightSources = lightSourceCount;
}

void ShaderGeneratorTester::validate(const mx::GenOptions& generateOptions, const std::string& optionsFilePath, bool enableUserData)
{
    // Start logging
    _logFile.open(_logFilePath);

    // Check for an option file
    TestSuiteOptions options;
    if (!options.readOptions(optionsFilePath))
    {
        _logFile << "Cannot read options file: " << optionsFilePath << ". Skipping test." << std::endl;
        _logFile.close();
        return;
    }
    // Test has been turned off so just do nothing.
    if (!runTest(options))
    {
        _logFile << "Language / target: " << _languageTargetString << " not set to run. Skipping test." << std::endl;
        _logFile.close();
        return;
    }
    options.print(_logFile);

    // Add files to override the files in the test suite to be examined.
    mx::StringSet overrideFiles;
    for (const auto& filterFile : options.overrideFiles)
    {
        overrideFiles.insert(filterFile);
    }

    // Dependent library setup
    setupDependentLibraries();
    addColorManagement();
    addUnitSystem();

    // Test suite setup
    addSkipFiles();

    // Generation setup
    setTestStages();

    // Load in all documents to test
    mx::StringVec errorLog;
    mx::FileSearchPath searchPath(_libSearchPath);
    mx::XmlReadOptions readOptions;
    readOptions.applyFutureUpdates = options.applyFutureUpdates;
    for (const auto& testRoot : _testRootPaths)
    {
        mx::loadDocuments(testRoot, searchPath, _skipFiles, overrideFiles, _documents, _documentPaths, 
                          readOptions, errorLog);
    }
    CHECK(errorLog.empty());
    for (const auto& error : errorLog)
    {
        _logFile << error << std::endl;
    }

    // Scan each document for renderable elements and check code generation
    //
    // Map to replace "/" in Element path names with "_".
    mx::StringMap pathMap;
    pathMap["/"] = "_";

    // Add nodedefs to skip when testing
    addSkipNodeDefs();

    // Create our context
    mx::GenContext context(_shaderGenerator);
    context.getOptions() = generateOptions;
    context.registerSourceCodeSearchPath(_srcSearchPath);

    // Define working unit if required
    if (context.getOptions().targetDistanceUnit.empty())
    {
        context.getOptions().targetDistanceUnit = _defaultDistanceUnit;
    }

    size_t documentIndex = 0;
    mx::CopyOptions copyOptions;
    copyOptions.skipConflictingElements = true;
    for (const auto& doc : _documents)
    {
        // Add in dependent libraries
        bool importedLibrary = false;
        try
        {
            doc->importLibrary(_dependLib, &copyOptions);
            importedLibrary = true;
        }
        catch (mx::Exception& e)
        {
            _logFile << "Failed to import library into file: " 
                    << _documentPaths[documentIndex] << ". Error: "
                    << e.what() << std::endl;
            CHECK(importedLibrary);
            continue;
        }

        if (options.applyFutureUpdates &&
            (!doc->getNodes(mx::SURFACE_MATERIAL_NODE_STRING).empty() ||
             !doc->getNodes(mx::VOLUME_MATERIAL_NODE_STRING).empty()))
        {
            _logFile << "Updated version document written to: " << doc->getSourceUri() + "modified.mtlxx" << std::endl;

            mx::StringSet xincludeFiles = doc->getReferencedSourceUris();
            auto skipXincludes = [xincludeFiles](mx::ConstElementPtr elem)
            {
                if (elem->hasSourceUri())
                {
                    return (xincludeFiles.count(elem->getSourceUri()) == 0);
                }
                return true;
            };
            mx::XmlWriteOptions writeOptions;
            writeOptions.writeXIncludeEnable = true;
            writeOptions.elementPredicate = skipXincludes;
            mx::writeToXmlFile(doc, doc->getSourceUri() + "_modified.mtlxx", &writeOptions);
        }

        // Find and register lights
        findLights(doc, _lights);
        registerLights(doc, _lights, context);


        // Add any userdata if required
        if (enableUserData)
        {
            addUserData(context);
        }

        // Find elements to render in the document
        std::vector<mx::TypedElementPtr> elements;
        try
        {
            mx::findRenderableElements(doc, elements);
        }
        catch (mx::Exception& e)
        {
            _logFile << "Renderables search errors: " << e.what() << std::endl;
        }

        if (!elements.empty())
        {
            _logFile << "MTLX Filename :" << _documentPaths[documentIndex] << ". Elements tested: "
                << std::to_string(elements.size()) << std::endl;
            documentIndex++;
        }

        // Perform document validation
        std::string message;
        bool docValid = doc->validate(&message);
        if (!docValid)
        {
            _logFile << "Document is invalid: [" << doc->getSourceUri() << "] " << message;
        }
        CHECK(docValid);

        // Traverse the renderable elements and run the validation step
        int missingNodeDefs = 0;
        int missingImplementations = 0;
        int codeGenerationFailures = 0;
        for (const auto& element : elements)
        {
            mx::TypedElementPtr targetElement = element;
            mx::OutputPtr output = targetElement->asA<mx::Output>();
            mx::ShaderRefPtr shaderRef = targetElement->asA<mx::ShaderRef>();
            mx::NodePtr outputNode = targetElement->asA<mx::Node>();
            mx::NodeDefPtr nodeDef = nullptr;
            if (output)
            {
                outputNode = output->getConnectedNode();
                // Handle connected upstream material nodes later on.
                if (outputNode->getType() != mx::MATERIAL_TYPE_STRING)
                {
                    nodeDef = outputNode->getNodeDef();
                }
            }
            else if (shaderRef)
            {
                nodeDef = shaderRef->getNodeDef();
            }

            // Handle material node checking. For now only check first surface shader if any
            if (outputNode && outputNode->getType() == mx::MATERIAL_TYPE_STRING)
            {
                std::vector<mx::NodePtr> shaderNodes = getShaderNodes(outputNode, mx::SURFACE_SHADER_TYPE_STRING);
                if (!shaderNodes.empty())
                {
                    nodeDef = shaderNodes[0]->getNodeDef();
                    targetElement = shaderNodes[0];
                }
            }

            const std::string namePath(targetElement->getNamePath());
            if (nodeDef)
            {
                // Allow to skip nodedefs to test if specified
                const std::string nodeDefName = nodeDef->getName();
                if (_skipNodeDefs.count(nodeDefName))
                {
                    _logFile << ">> Skipped testing nodedef: " << nodeDefName << std::endl;
                    continue;
                }

                mx::string elementName = mx::replaceSubstrings(namePath, pathMap);
                elementName = mx::createValidName(elementName);

                mx::InterfaceElementPtr impl = nodeDef->getImplementation(_shaderGenerator->getTarget(), _shaderGenerator->getLanguage());
                if (impl)
                {
                    // Record implementations tested
                    if (options.checkImplCount)
                    {
                        mx::NodeGraphPtr nodeGraph = impl->asA<mx::NodeGraph>();
                        mx::InterfaceElementPtr nodeGraphImpl = nodeGraph ? nodeGraph->getImplementation() : nullptr;
                        _usedImplementations.insert(nodeGraphImpl ? nodeGraphImpl->getName() : impl->getName());
                    }

                    _logFile << "------------ Run validation with element: " << namePath << "------------" << std::endl;
                    mx::StringVec sourceCode;
                    bool generatedCode = generateCode(context, elementName, targetElement, _logFile, _testStages, sourceCode);
                    if (!generatedCode)
                    {
                        _logFile << ">> Failed to generate code for nodedef: " << nodeDefName << std::endl;
                        codeGenerationFailures++;
                    }
                    else if (_writeShadersToDisk && sourceCode.size())
                    {
                        const std::string udsuffix(enableUserData ? TEST_USER_DATA_SUFFIX : mx::EMPTY_STRING);

                        mx::FilePath path = element->getActiveSourceUri();
                        if (!path.isEmpty())
                        {
                            std::string testFileName = path[path.size() - 1];
                            size_t pos = testFileName.rfind('.');
                            if (pos != std::string::npos)
                                testFileName = testFileName.substr(0, pos);

                            path = path.getParentPath() / testFileName;
                            if (!path.exists())
                            {
                                path.createDirectory();
                            }
                        }
                        else
                        {
                            path = mx::FilePath::getCurrentPath();
                        }
                        path = path / "generatedshaders";
                        if (!path.exists())
                        {
                            path.createDirectory();
                        }
                        path = path / _shaderGenerator->getLanguage();
                        if (!path.exists())
                        {
                            path.createDirectory();
                        }
                        path = path / _shaderGenerator->getTarget();
                        if (!path.exists())
                        {
                            path.createDirectory();
                        }
                        
                        std::vector<mx::FilePath> sourceCodePaths;
                        if (sourceCode.size() > 1)
                        {
                            for (size_t i=0; i<sourceCode.size(); ++i)
                            {
                                const mx::FilePath filename = path / (elementName + udsuffix + "." + _testStages[i] + "." + getFileExtensionForLanguage(_shaderGenerator->getLanguage()));
                                sourceCodePaths.push_back(filename);
                                std::ofstream file(filename.asString());
                                file << sourceCode[i];
                                file.close();
                            }
                        }
                        else
                        {
                            path = path / (elementName + "." + getFileExtensionForLanguage(_shaderGenerator->getLanguage()));
                            sourceCodePaths.push_back(path);
                            std::ofstream file(path.asString());
                            file << sourceCode[0];
                            file.close();
                        }

                        // Run compile test
                        compileSource(sourceCodePaths);
                    }
                }
                else
                {
                    _logFile << ">> Failed to find implementation for nodedef: " << nodeDefName << std::endl;
                    missingImplementations++;
                }
            }
            else
            {
                _logFile << ">> Failed to find nodedef for: " << namePath << std::endl;
                missingNodeDefs++;
            }
        }

        CHECK(missingNodeDefs == 0);
        CHECK(missingImplementations == 0);
        CHECK(codeGenerationFailures == 0);
    }

    if (options.checkImplCount)
    {
        _logFile << "---------------------------------------------------" << std::endl;
        checkImplementationUsage(_usedImplementations, context, _logFile);
    }

    // End logging
    if (_logFile.is_open())
    {
        _logFile.close();
    }
}

void TestSuiteOptions::print(std::ostream& output) const
{
    output << "Render Test Options:" << std::endl;
    output << "\tApply future updates: " << std::to_string(applyFutureUpdates) << std::endl;
    output << "\tOverride Files: { ";
    for (const auto& overrideFile : overrideFiles) { output << overrideFile << " "; }
    output << "} " << std::endl;
    output << "\tLight Setup Files: { ";
    for (const auto& lightFile : lightFiles) { output << lightFile << " "; }
    output << "} " << std::endl;
    output << "\tLanguage / Targets to run: " << std::endl;
    for (const auto& l : languageAndTargets)
    {
        mx::StringVec languageAndTarget = mx::splitString(l, "_");
        size_t count = languageAndTarget.size();
        output << "\t\tLanguage: " << ((count > 0) ? languageAndTarget[0] : "NONE") << ". ";
        output << "Target: " << ((count > 1) ? languageAndTarget[1] : "NONE");
        output << std::endl;
    }
    output << "\tCheck Implementation Usage Count: " << checkImplCount << std::endl;
    output << "\tDump Generated Code: " << dumpGeneratedCode << std::endl;
    output << "\tShader Interfaces: " << shaderInterfaces << std::endl;
    output << "\tValidate Element To Render: " << validateElementToRender << std::endl;
    output << "\tCompile code: " << compileCode << std::endl;
    output << "\tRender Images: " << renderImages << std::endl;
    output << "\tRender Size: " << renderSize[0] << "," << renderSize[1] << std::endl;
    output << "\tSave Images: " << saveImages << std::endl;
    output << "\tDump uniforms and Attributes  " << dumpUniformsAndAttributes << std::endl;
    output << "\tNon-Shaded Geometry: " << unShadedGeometry.asString() << std::endl;
    output << "\tShaded Geometry: " << shadedGeometry.asString() << std::endl;
    output << "\tGeometry Scale: " << geometryScale << std::endl;
    output << "\tEnable Direct Lighting: " << enableDirectLighting << std::endl;
    output << "\tEnable Indirect Lighting: " << enableIndirectLighting << std::endl;
    output << "\tSpecular Environment Method: " << specularEnvironmentMethod << std::endl;
    output << "\tRadiance IBL File Path " << radianceIBLPath.asString() << std::endl;
    output << "\tIrradiance IBL File Path: " << irradianceIBLPath.asString() << std::endl;
    output << "\tExternal library paths: " << externalLibraryPaths.asString() << std::endl;
    output << "\tExternal test root paths: " << externalTestPaths.asString() << std::endl;
}

bool TestSuiteOptions::readOptions(const std::string& optionFile)
{
    // These strings should make the input names defined in the
    // GenShaderUtil::TestSuiteOptions nodedef in test suite file _options.mtlx
    //
    const std::string RENDER_TEST_OPTIONS_STRING("TestSuiteOptions");
    const std::string OVERRIDE_FILES_STRING("overrideFiles");
    const std::string LANGUAGE_AND_TARGETS_STRING("languageAndTargets");
    const std::string LIGHT_FILES_STRING("lightFiles");
    const std::string SHADER_INTERFACES_STRING("shaderInterfaces");
    const std::string VALIDATE_ELEMENT_TO_RENDER_STRING("validateElementToRender");
    const std::string COMPILE_CODE_STRING("compileCode");
    const std::string RENDER_IMAGES_STRING("renderImages");
    const std::string RENDER_SIZE_STRING("renderSize");
    const std::string SAVE_IMAGES_STRING("saveImages");
    const std::string DUMP_UNIFORMS_AND_ATTRIBUTES_STRING("dumpUniformsAndAttributes");
    const std::string CHECK_IMPL_COUNT_STRING("checkImplCount");
    const std::string DUMP_GENERATED_CODE_STRING("dumpGeneratedCode");
    const std::string UNSHADED_GEOMETRY_STRING("unShadedGeometry");
    const std::string SHADED_GEOMETRY_STRING("shadedGeometry");
    const std::string GEOMETRY_SCALE_STRING("geometryScale");
    const std::string ENABLE_DIRECT_LIGHTING("enableDirectLighting");
    const std::string ENABLE_INDIRECT_LIGHTING("enableIndirectLighting");
    const std::string SPECULAR_ENVIRONMENT_METHOD("specularEnvironmentMethod");
    const std::string RADIANCE_IBL_PATH_STRING("radianceIBLPath");
    const std::string IRRADIANCE_IBL_PATH_STRING("irradianceIBLPath");
    const std::string TRANSFORM_UVS_STRING("transformUVs");
    const std::string SPHERE_OBJ("sphere.obj");
    const std::string SHADERBALL_OBJ("shaderball.obj");
    const std::string EXTERNAL_LIBRARY_PATHS("externalLibraryPaths");
    const std::string EXTERNAL_TEST_PATHS("externalTestPaths");
    const std::string APPLY_LATEST_UPDATES("applyFutureUpdates");

    overrideFiles.clear();
    dumpGeneratedCode = false;
    unShadedGeometry = SPHERE_OBJ;
    shadedGeometry = SHADERBALL_OBJ;
    geometryScale = 1.0f;
    enableDirectLighting = true;
    enableIndirectLighting = true;
    specularEnvironmentMethod = mx::SPECULAR_ENVIRONMENT_FIS;
    applyFutureUpdates = false;

    MaterialX::DocumentPtr doc = MaterialX::createDocument();
    try {
        mx::XmlReadOptions readOptions;
        MaterialX::readFromXmlFile(doc, optionFile, mx::FileSearchPath(), &readOptions);

        MaterialX::NodeDefPtr optionDefs = doc->getNodeDef(RENDER_TEST_OPTIONS_STRING);
        if (optionDefs)
        {
            for (MaterialX::ParameterPtr p : optionDefs->getParameters())
            {
                const std::string& name = p->getName();
                MaterialX::ValuePtr val = p->getValue();
                if (val)
                {
                    if (name == OVERRIDE_FILES_STRING)
                    {
                        overrideFiles = MaterialX::splitString(p->getValueString(), ",");
                    }
                    else if (name == LIGHT_FILES_STRING)
                    {
                        lightFiles = MaterialX::splitString(p->getValueString(), ",");
                    }
                    else if (name == SHADER_INTERFACES_STRING)
                    {
                        shaderInterfaces = val->asA<int>();
                    }
                    else if (name == VALIDATE_ELEMENT_TO_RENDER_STRING)
                    {
                        validateElementToRender = val->asA<bool>();
                    }
                    else if (name == COMPILE_CODE_STRING)
                    {
                        compileCode = val->asA<bool>();
                    }
                    else if (name == RENDER_IMAGES_STRING)
                    {
                        renderImages = val->asA<bool>();
                    }
                    else if (name == RENDER_SIZE_STRING)
                    {
                        renderSize = val->asA<mx::Vector2>();
                    }
                    else if (name == SAVE_IMAGES_STRING)
                    {
                        saveImages = val->asA<bool>();
                    }
                    else if (name == DUMP_UNIFORMS_AND_ATTRIBUTES_STRING)
                    {
                        dumpUniformsAndAttributes = val->asA<bool>();
                    }
                    else if (name == LANGUAGE_AND_TARGETS_STRING)
                    {
                        mx::StringVec list = mx::splitString(p->getValueString(), ",");
                        for (const auto& l : list)
                        {
                            languageAndTargets.insert(l);
                        }
                    }
                    else if (name == CHECK_IMPL_COUNT_STRING)
                    {
                        checkImplCount = val->asA<bool>();
                    }
                    else if (name == DUMP_GENERATED_CODE_STRING)
                    {
                        dumpGeneratedCode = val->asA<bool>();
                    }
                    else if (name == UNSHADED_GEOMETRY_STRING)
                    {
                        unShadedGeometry = p->getValueString();
                    }
                    else if (name == SHADED_GEOMETRY_STRING)
                    {
                        shadedGeometry = p->getValueString();
                    }
                    else if (name == GEOMETRY_SCALE_STRING)
                    {
                        geometryScale = val->asA<float>();
                    }
                    else if (name == ENABLE_DIRECT_LIGHTING)
                    {
                        enableDirectLighting = val->asA<bool>();
                    }
                    else if (name == ENABLE_INDIRECT_LIGHTING)
                    {
                        enableIndirectLighting = val->asA<bool>();
                    }
                    else if (name == SPECULAR_ENVIRONMENT_METHOD)
                    {
                        specularEnvironmentMethod = (mx::HwSpecularEnvironmentMethod) val->asA<int>();
                    }
                    else if (name == RADIANCE_IBL_PATH_STRING)
                    {
                        radianceIBLPath = p->getValueString();
                    }
                    else if (name == IRRADIANCE_IBL_PATH_STRING)
                    {
                        irradianceIBLPath = p->getValueString();
                    }
                    else if (name == TRANSFORM_UVS_STRING)
                    {
                        transformUVs = val->asA<mx::Matrix44>();
                    }
                    else if (name == EXTERNAL_LIBRARY_PATHS)
                    {
                        mx::StringVec list = mx::splitString(p->getValueString(), ",");
                        for (const auto& l : list)
                        {
                            externalLibraryPaths.append(mx::FilePath(l));
                        }
                    }
                    else if (name == EXTERNAL_TEST_PATHS)
                    {
                        mx::StringVec list = mx::splitString(p->getValueString(), ",");
                        for (const auto& l : list)
                        {
                            externalTestPaths.append(mx::FilePath(l));
                        }
                    }
                    else if (name == APPLY_LATEST_UPDATES)
                    {
                        applyFutureUpdates = p->getValue()->asA<bool>();
                    }
                }
            }
        }

        // Disable render and save of images if not compiled code will be generated
        if (!compileCode)
        {
            renderImages = false;
            saveImages = false;
        }
        // Disable saving images, if no images are to be produced
        if (!renderImages)
        {
            saveImages = false;
        }
        // Disable direct lighting
        if (!enableDirectLighting)
        {
            lightFiles.clear();
        }
        // Disable indirect lighting
        if (!enableIndirectLighting)
        {
            radianceIBLPath.assign(mx::EMPTY_STRING);
            irradianceIBLPath.assign(mx::EMPTY_STRING);
        }

        // If there is a filter on the files to run turn off profile checking
        if (!overrideFiles.empty())
        {
            checkImplCount = false;
        }
        return true;
    }
    catch (mx::Exception& e)
    {
        std::cout << e.what();
    }
    return false;
}

} // namespace GenShaderUtil
