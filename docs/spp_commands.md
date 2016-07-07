spp commands:

* help [> help]

  Displays brief help


* status [> status]

  Display number of connected primary and secondary application count
  Also display connected secondary application client_id


* sec <id>

  Send commands to secondary applications with client id <id>:

  * status [> sec 0;status]

  * add

    * ring <id>  [> sec 0;add ring 0]

    * vhost <id> [> sec 0;add vhost 0]

  * patch <id> <id> [> sec 0;patch 0 2]

    * reset [> sec 0; patch reset]

  * forward [> sec 0;forward]

  * del

    * ring <id> [> sec 0; del ring 0]

  * exit [> sec 0;exit]

  * stop [>sec 0;stop]


* record, playback [> record a.txt] [> playback a.txt]

  To record commands in SPP:-
  spp > record <filename>

  To stop recording
  spp > bye

  To playback commands
  spp > playback <filename>

  '#' character at the beginning of the line used for comments


* bye [> bye]

  Close the spp

  * sec [> bye sec]

    Close all secondary app and spp

  * all [> bye all]

    Close all secondary app, primary app and spp


* pri

  Send commands to primary applications:

  * status [> pri status]

  * add

    * ring <id> [> pri add ring 0]

  * del

    * ring <id> [> pri del ring 0]

  * exit [> pri exit]

  * stop [> pri stop]

  * start [> pri start]

  * clear [> pri clear]

    Clear statistics
