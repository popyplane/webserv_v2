<?php
header("Content-Type: text/html");
header("X-CGI-Test-Header: My Custom Value");
echo "<html><body>";
echo "<h1>Hello from PHP CGI!</h1>";
echo "<p>Request Method: " . $_SERVER['REQUEST_METHOD'] . "</p>";
echo "<p>Script Name: " . $_SERVER['SCRIPT_NAME'] . "</p>";
echo "<p>Path Info: " . $_SERVER['PATH_INFO'] . "</p>";
echo "<p>Query String: " . $_SERVER['QUERY_STRING'] . "</p>
";
if ($_SERVER['REQUEST_METHOD'] == 'POST') {
    echo "<h2>POST Body Received:</h2>";
    $input = file_get_contents('php://stdin');
    if ($input === false) { echo "<p>Error reading POST body.</p>"; } 
    elseif (empty($input)) { echo "<p>No POST body provided.</p>"; }
    else { echo "<pre>" . htmlspecialchars($input) . "</pre>"; }
}
echo "<h2>All Environment Variables:</h2>";
echo "<pre>";
foreach ($_SERVER as $key => $value) {
    echo $key . " = " . htmlspecialchars($value) . "\n";
}
echo "</pre>";
echo "</body></html>";
?>