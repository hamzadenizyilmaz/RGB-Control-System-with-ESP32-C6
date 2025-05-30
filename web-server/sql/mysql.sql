CREATE TABLE color_settings (
    id INT AUTO_INCREMENT PRIMARY KEY,
    red INT NOT NULL,
    green INT NOT NULL,
    blue INT NOT NULL,
    brightness INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE relay_states (
    id INT AUTO_INCREMENT PRIMARY KEY,
    state TINYINT(1) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);