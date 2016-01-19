var INITIALIZER = '__init__';

function isPlainObject(obj) {
    return Object.getPrototypeOf(obj) === Object.prototype;
}

function to_python_case(key) {
    return key.replace(/([a-z])([A-Z]+)/g, function ($0, $1, $2) {
        return $1 + '_' + $2.toLowerCase();
    });
}

function toCamelCase(key) {
    return key.replace(/[_-]([a-z])/g, function ($0, $1) {
        return $1.toUpperCase();
    });
}

function withoutCamelCase(obj) {
    var out = {}, key;
    for (key in obj) {
        out[to_python_case(key)] = obj[key];
    }
    return out;
}

function wrapInitializer(init) {
    return function () {
        for (var
            tmp,
            length = arguments.length,
            args = [],
            i = 0; i < length; i++
        ) {
            tmp = arguments[i];
            args[i] = isPlainObject(tmp) ? withoutCamelCase(tmp) : tmp;
        }
        init.apply(this, args);
    };
}

exports.withCamelCase = function withCamelCase(ns) {
    var descriptors = {};
    var is_python_case = /[a-z]_[a-z]/;
    var isConstructor = /^[A-Z]/;
    Object.getOwnPropertyNames(ns).forEach(function (key) {
        var descriptor = Object.getOwnPropertyDescriptor(ns, key);
        var isDataDescriptor = descriptor.hasOwnProperty('value');
        var isFunction = isDataDescriptor && typeof descriptor.value === 'function';
        if (is_python_case.test(key)) {
            descriptors[toCamelCase(key)] = descriptor;
        } else if (isFunction) {
            if (key === INITIALIZER) {
                descriptor.value = wrapInitializer(descriptor.value);
                descriptors[key] = descriptor;
            } else if (isConstructor.test(key)) {
                withCamelCase(descriptor.value);
                withCamelCase(descriptor.value.prototype);
            }
        }
    });
    return Object.defineProperties(ns, descriptors);
};