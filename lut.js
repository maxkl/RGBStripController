/**
 * Script for generating C/C++ look-up tables
 */

const confs = {
    'pwm_1024': {
        xMin: 0,
        xMax: 1023,
        f: x => 0.000977517 * Math.pow(x, 2)
    },
    'pwm_65536': {
        xMin: 0,
        xMax: 1023,
        f: x => 0.0626213 * Math.pow(x, 2)
    },
    'sin': {
        xMin: 0,
        xMax: 255,
        f: x => (Math.sin(x * 2 * Math.PI / 1023) + 1) * 1023 / 2
    }
};

const confName = (process.argv.length >= 3) ? process.argv[2].toLowerCase() : 'pwm_1024';
const conf = confs[confName];

for (let x = conf.xMin; x <= conf.xMax; x++) {
    process.stdout.write(Math.round(conf.f(x)).toString());

    if (x < conf.xMax) {
        process.stdout.write(',');
    }
}
