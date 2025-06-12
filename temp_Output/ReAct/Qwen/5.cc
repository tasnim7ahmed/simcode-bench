#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create a point-to-point helper and set the link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on the nodes
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Enable packet capture on both devices
    p2p.EnablePcapAll("traffic_capture");

    // Run the simulation (no events scheduled, just capturing background traffic)
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}