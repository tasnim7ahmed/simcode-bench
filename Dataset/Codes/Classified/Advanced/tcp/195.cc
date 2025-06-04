#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkTransferExample");

int main(int argc, char *argv[])
{
    double simulationTime = 10.0; // Simulation time in seconds
    std::string dataRate = "5Mbps"; // Link data rate
    std::string delay = "2ms";      // Link delay

    // Enable logging
    LogComponentEnable("TcpBulkTransferExample", LOG_LEVEL_INFO);

    // Create two nodes (client and server)
    NodeContainer nodes;
    nodes.Create(2);

    // Set up a point-to-point link with the specified data rate and delay
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    // Install devices and create the point-to-point link
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up a TCP bulk send application on the client node
    uint16_t port = 9; // Port number for the TCP connection
    BulkSendHelper bulkSend("ns3::TcpSocketFactory",
                            InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    ApplicationContainer clientApp = bulkSend.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));

    // Set up a packet sink on the server node to receive the data
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // Enable pcap tracing
    pointToPoint.EnablePcap("tcp-bulk-transfer", devices.Get(0), true);

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Log the total number of bytes received by the server
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApp.Get(0));
    NS_LOG_INFO("Total Bytes Received by Server: " << sink->GetTotalRx());

    Simulator::Destroy();
    return 0;
}

