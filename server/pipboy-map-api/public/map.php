<?php

declare(strict_types=1);

require dirname(__DIR__) . DIRECTORY_SEPARATOR . 'src' . DIRECTORY_SEPARATOR . 'MapEndpoint.php';

PipboyMapEndpoint::run(dirname(__DIR__) . DIRECTORY_SEPARATOR . 'config.php');
