#ShrimpMonitor

ShrimpMonitor is a platform for monitoring water quality attributes specifically for indoor shrimp aquaculture.

The system uses an ATMega based microcontroller, and environmental sensors to monitor attributes such as:

* Dissolved Oxygen
* Temperature
* pH
* Salinity
* Oxidation Reduction Potential
* Turbidity (soon to be added)

The microcontroller writes sensor information to an AWS (Amazon Web Services) SQS queue.

##Components

ShrimpMonitor uses many open source tools.  The web front end is written in Ruby on Rails, with D3JS for graphing of data, using Twitter Bootstrap for responsive layout.

##Documentation

Is essentially non existent so far - will add when I have more time.

##Tests

Have not written any tests yet, on the todo list.

##License
[GPLv3] (http://www.gnu.org/licenses/gpl-3.0.html)
