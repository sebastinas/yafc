<?php
$page_title = 'Home';
require_once 'head.php';
?>


<p>Yafc is a fork of the old (dead?) <a href="http://yafc.sourceforge.net/">sourceforge project</a> of the same name.
Like the original project, It's <a href="http://www.gnu.org/licenses/gpl-2.0.html">GPL 2</a>.</p>

<h2>Features</h2>
<ul>
<li>cached directory listings
<li>extensive tab completion
<li>multiple connections open
<li>automatic reconnect on timed out connections
<li>aliases
<li>colored ls (ie, ls --color, uses $LS_COLORS like GNU ls)
<li>autologin and bookmarks
<li>Kerberos support (version 4 and 5, heimdal, kth-krb or MIT)
<li>SFTP support (SSH2)
<li>recursive get/put/fxp/rm/ls
<li>nohup mode get and put
<li>tagging (queueing) of files for later transferring
<li>automatically enters nohup-mode when SIGHUP received (in get and put)
<li>redirection to local command or file ('>', '>>' and '|')
<li>proxy support
</ul>

<h2>Download</h2>
<p>The source is located on <a href="https://github.com/sebastinas/yafc">GitHub</a>.
We also do releases from time to time.</p>

<h2>Bugs</h2>
<p>Please report bugs to <a href="https://github.com/sebastinas/yafc/issues">https://github.com/sebastinas/yafc/issues</a>.</p>
<p>Before reporting a bug, please verify you're using the latest version. When
reporting bugs, please include information on what machine and
operating system, including versions, you are running, an example for
me to reproduce (use the --trace option) and a patch if you have one.</p>

<h2>News</h2>
<p>Our first release (1.1.3) is due for release soon.</p>


<?php
require_once 'foot.php';
?>
