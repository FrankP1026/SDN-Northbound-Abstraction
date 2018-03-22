# SDN Northbound Abstraction

Part of CSC2229 Class Project done together with Gagandeep Singh (https://www.linkedin.com/in/gaganbahga/) when studying at University of Toronto for my Master of Engineering degree. Developed between 2015-09 and 2015-12.

In this part of the project, we developed northbound abstraction for control applications of Software-defined Networks (SDN) based on REST API. An application wrapper is designed to act as a command proxy for one-to-many communication between an application and controllers. It  is written using C language. 

After receiving a REST command from an application, the wrapper decomposes and dispatches REST commands to the designated controllers based on its subnet-to-controller mapping dictionary. Every wrapper organizes and maintains such a dictionary. After receiving the possible responses from designated controllers, the wrapper composes them as a single response and push it to the application.

Since REST commands are very diversified, we tried to apply this solution to 7 REST API commands of Floodlight controller.
