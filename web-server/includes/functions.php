<?php
function sanitizeInput($data) {
    return htmlspecialchars(strip_tags(trim($data)));
}

function validateApiKey($key) {
    return $key == API_KEY;
}
?>