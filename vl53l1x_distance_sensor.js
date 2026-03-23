const {
    identify,
    has_diagnostic_entities,
} = require('zigbee-herdsman-converters/lib/modernExtend');

const definition = {
    zigbeeModel: ['ESP32C3_VL53L1X'], // Musi pasować do nazwy raportowanej przez ESP
    model: 'VL53L1X_Distance',
    vendor: 'Custom_ESP32',
    description: 'Laserowy czujnik odległości VL53L1X',
    extend: [
        identify(),
    ],
    expose: [
        // Definiujemy nasz sensor odległości
        {
            type: 'numeric',
            name: 'distance',
            unit: 'mm',
            cluster: 0xFC00,
            attribute: {ID: 0x0001, type: 0x21}, // 0x21 to typ U16 (Uint16)
            description: 'Zmierzona odległość w milimetrach',
            access: 'STATE',
        },
    ],
    fromZigbee: [
        {
            cluster: '64512', // 0xFC00 w formacie decymalnym
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                if (msg.data.hasOwnProperty(1)) { // Nasz atrybut 0x0001
                    return {distance: msg.data[1]};
                }
            },
        },
    ],
    toZigbee: [],
};

module.exports = definition;
