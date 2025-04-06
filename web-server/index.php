<?php
require_once 'api/config.php';
?>
<!DOCTYPE html>
<html>
<head>
    <title>LED Kontrol Paneli</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }
        .color-picker { margin: 20px 0; }
        .color-preview { width: 100px; height: 100px; border: 1px solid #000; }
    </style>
</head>
<body>
    <h1>RGB LED Kontrol Paneli</h1>
    
    <div class="color-picker">
        <div class="color-preview" id="colorPreview"></div>
        <p>Kırmızı: <input type="range" id="red" min="0" max="255" value="255"></p>
        <p>Yeşil: <input type="range" id="green" min="0" max="255" value="0"></p>
        <p>Mavi: <input type="range" id="blue" min="0" max="255" value="0"></p>
        <p>Parlaklık: <input type="range" id="brightness" min="0" max="100" value="100"></p>
        <button onclick="updateColor()">Renk Güncelle</button>
    </div>
    
    <div class="relay-control">
        <h2>Röle Kontrol</h2>
        <button onclick="setRelay(1)">AÇ</button>
        <button onclick="setRelay(0)">KAPAT</button>
        <p>Durum: <span id="relayState">KAPALI</span></p>
    </div>

    <script>
        function updateColor() {
            const data = {
                red: document.getElementById('red').value,
                green: document.getElementById('green').value,
                blue: document.getElementById('blue').value,
                brightness: document.getElementById('brightness').value
            };
            
            fetch('/api/color.php', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'X-API-KEY': '<?php echo API_KEY; ?>'
                },
                body: JSON.stringify(data)
            })
            .then(response => response.json())
            .then(data => {
                document.getElementById('colorPreview').style.backgroundColor = 
                    `rgb(${data.red}, ${data.green}, ${data.blue})`;
            });
        }
        
        function setRelay(state) {
            fetch('/api/relay.php', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'X-API-KEY': '<?php echo API_KEY; ?>'
                },
                body: JSON.stringify({ state: state })
            })
            .then(response => response.json())
            .then(data => {
                document.getElementById('relayState').innerText = 
                    data.state ? 'AÇIK' : 'KAPALI';
            });
        }
        
        window.onload = function() {
            fetch('/api/relay.php', {
                headers: { 'X-API-KEY': '<?php echo API_KEY; ?>' }
            })
            .then(response => response.json())
            .then(data => {
                document.getElementById('relayState').innerText = 
                    data.state ? 'AÇIK' : 'KAPALI';
            });
            
            fetch('/api/color.php', {
                headers: { 'X-API-KEY': '<?php echo API_KEY; ?>' }
            })
            .then(response => response.json())
            .then(data => {
                document.getElementById('red').value = data.red;
                document.getElementById('green').value = data.green;
                document.getElementById('blue').value = data.blue;
                document.getElementById('brightness').value = data.brightness;
                document.getElementById('colorPreview').style.backgroundColor = 
                    `rgb(${data.red}, ${data.green}, ${data.blue})`;
            });
        };
    </script>
</body>
</html>