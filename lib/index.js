
var gi;
try {
    gi = require('../build/Release/node-gtk');
} catch(e) {
    gi = require('../build/Debug/node-gtk');
}

// The bootstrap from C here contains functions and methods for each object,
// namespaced with underscores. See gi.cc for more information.
var GIRepository = gi.Bootstrap();

// The GIRepository API is fairly poor, and contains methods on classes,
// methods on objects, and what should be methods interpreted as functions,
// because the scanner does not interpret methods on typedefs correctly.

// We extend this bootstrap'd repo to define all flags / enums, which
// are all we need to start declaring objects.
(function() {
    var repo = GIRepository.Repository_get_default();
    var ns = "GIRepository";

    // First, grab InfoType so we can find enums / flags.
    var InfoType = makeEnum(GIRepository.Repository_find_by_name.call(repo, ns, "InfoType"));

    // Now, define all enums / flags.
    var nInfos = GIRepository.Repository_get_n_infos.call(repo, ns);
    for (var i = 0; i < nInfos; i++) {
        var info = GIRepository.Repository_get_info.call(repo, ns, i);
        var name = GIRepository.BaseInfo_get_name.call(info);
        var type = GIRepository.BaseInfo_get_type.call(info);
        if (type === InfoType.ENUM || type === InfoType.FLAGS)
            GIRepository[name] = makeEnum(info);
    }
})();

function declareFunction(obj, info) {
    var name = GIRepository.BaseInfo_get_name.call(info);
    var flags = GIRepository.function_info_get_flags(info);
    var func = gi.MakeFunction(info);
    var target = flags & GIRepository.FunctionInfoFlags.IS_METHOD ? obj.prototype : obj;
    Object.defineProperty(target, toCamelCase(name), {
        configurable: true,
        writable: true,
        value: func
    });
}

function makeEnum(info) {
    var obj = {};
    var nValues = GIRepository.enum_info_get_n_values(info);

    for (var i = 0; i < nValues; i++) {
        var valueInfo = GIRepository.enum_info_get_value(info, i);
        var valueName = GIRepository.BaseInfo_get_name.call(valueInfo);
        var valueValue = GIRepository.value_info_get_value(valueInfo);
        obj[valueName.toUpperCase()] = valueValue;
    }

    return obj;
}

function makeConstant(info) {
    return gi.GetConstantValue(info);
}

function makeFunction(info) {
    return gi.MakeFunction(info);
}

function makeObject(info) {
    function propertyGetter(propertyName) {
        return function() {
            return gi.ObjectPropertyGetter(this, propertyName);
        };
    }
    function propertySetter(propertyName) {
        return function(value) {
            return gi.ObjectPropertySetter(this, propertyName, value);
        };
    }

    var constructor = gi.MakeClass(info);

    var nMethods = GIRepository.object_info_get_n_methods(info);
    for (var i = 0; i < nMethods; i++) {
        var methodInfo = GIRepository.object_info_get_method(info, i);
        declareFunction(constructor, methodInfo);
    }

    var nProperties = GIRepository.object_info_get_n_properties(info);
    for (var i = 0; i < nProperties; i++) {
        var propertyInfo = GIRepository.object_info_get_property(info, i);
        var propertyName = GIRepository.BaseInfo_get_name.call(propertyInfo);
        Object.defineProperty(constructor.prototype, toCamelCase(propertyName), {
            configurable: true,
            get: propertyGetter(propertyName),
            set: propertySetter(propertyName)
          }
        );
    }

    return constructor;
}

function makeInfo(info) {
    var type = GIRepository.BaseInfo_get_type.call(info);

    if (type === GIRepository.InfoType.ENUM)
        return makeEnum(info);
    if (type === GIRepository.InfoType.CONSTANT)
        return makeConstant(info);
    if (type === GIRepository.InfoType.FUNCTION)
        return makeFunction(info);
    if (type === GIRepository.InfoType.OBJECT)
        return makeObject(info);
}

function importNS(ns, version) {
    var module = {};

    var repo = GIRepository.Repository_get_default();
    GIRepository.Repository_require.call(repo, ns, version || null, 0);

    var nInfos = GIRepository.Repository_get_n_infos.call(repo, ns);
    for (var i = 0; i < nInfos; i++) {
        var info = GIRepository.Repository_get_info.call(repo, ns, i);
        var name = GIRepository.BaseInfo_get_name.call(info);
        // beside constants, everything should be camel Case
        if (!/^[A-Z_]+$/.test(name)) name = toCamelCase(name);
        module[name] = makeInfo(info);
        // in case the export is a constructor
        if (typeof module[name] === 'function' && /^[A-Z]/.test(name)) {
            // export its camelCase aware counterpart
            module[name] = wrapConstructor(module[name]);
        }
    }

    var override;
    try {
        override = require('./overrides/' + ns);
    } catch (e) {
        // No override
    }

    if (override)
        override.apply(module);

    return module;
}

function toCamelCase(key) {
  return key.replace(/[_-]([a-z])/g, function ($0, $1) {
    return $1.toUpperCase();
  });
}

function deCamelCase(key) {
  return key.replace(/([a-z])([A-Z]+)/g, function ($0, $1, $2) {
    return $1 + '_' + $2.toLowerCase();
  });
}

// It makes it possible to create instances directly
// witohut using the `new` keyword.
// e.g. factory[3].apply(Constructor, [1, 2, 3]);
// Returns the result of `new Constructor(1, 2, 3)` 
var factory = createFactory(10);
function createFactory(maxAmountOfArguments) {
    for (var
        length = maxAmountOfArguments + 1,
        factory = [],
        args = function (length) {
            var out = [];
            for (var i = 0; i < length; i++) {
                out[i] = 'arguments[' + i + ']';
            }
            return out.join(',');
        },
        i = 0; i < length; i++
    ) {
        factory[i] = Function('return new this(' + args(i) + ')');
    }
    return factory;
}

// Returns true if an instanceof Object is
// actually just an Object
var isObject = (function (toString) {
    var reference = toString.call({});
    return function isObject(object) {
        return toString.call(object) === reference;
    };
}(Object.prototype.toString));

function withCamelCaseProperties(obj) {
    var out = {};
    for (var key in obj)
        out[deCamelCase(key)] = obj[key];
    return out;
}

function wrapConstructor(Constructor) {
    function NodeGtkClass() {
        for (var
            length = arguments.length,
            args = [],
            i = 0; i < length; i++
        ) {
            args[i] = withCamelCaseProperties(arguments[i]);
        }
        return factory[length].apply(Constructor, args);
    }
    NodeGtkClass.prototype = Constructor.prototype;
    Object.defineProperties(
        NodeGtkClass,
        Object.getOwnPropertyNames(Constructor).reduce(function (d, key) {
          d[key] = Object.getOwnPropertyDescriptor(Constructor, key);
          return d;
        }, {})
    );
    return NodeGtkClass;
}

// Bootstrap step.

(function() {
    var repo = GIRepository.Repository_get_default();
    var ns = "GIRepository";

    // First, grab InfoType.
    var InfoType = makeEnum(GIRepository.Repository_find_by_name.call(repo, ns, "InfoType"));

    // Now, define all enums / flags.
    var nInfos = GIRepository.Repository_get_n_infos.call(repo, ns);
    for (var i = 0; i < nInfos; i++) {
        var info = GIRepository.Repository_get_info.call(repo, ns, i);
        var name = GIRepository.BaseInfo_get_name.call(info);
        var type = GIRepository.BaseInfo_get_type.call(info);
        if (type === InfoType.ENUM || type === InfoType.FLAGS)
            GIRepository[name] = makeEnum(info);
    }
})();

exports.importNS = function(ns, version) {
    return importNS(ns, version);
};

exports.startLoop = function() {
    gi.StartLoop();
};
