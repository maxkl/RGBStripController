/**
 * Script for generating C/C++ look-up tables
 */

const confs = {
    'pwm': {
        xMin: 0,
        xMax: 1023,
        f: x => 0.0626213 * Math.pow(x, 2)
    },
    'pwm_linear': {
        xMin: 0,
        xMax: 1023,
        f: x => (65535 / 1023) * x
    },
    'sin': {
        xMin: 0,
        xMax: 255,
        f: x => (Math.sin(x * 2 * Math.PI / 1023) + 1) * 1023 / 2
    }
};

const confName = (process.argv.length >= 3) ? process.argv[2].toLowerCase() : 'pwm';
const conf = confs[confName];

for (let x = conf.xMin; x <= conf.xMax; x++) {
    process.stdout.write(Math.round(conf.f(x)).toString());

    if (x < conf.xMax) {
        process.stdout.write(',');
    }
}
