#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSimulation");

int main(int argc, char *argv[]) {
    // Enable logging for UdpEchoClient and UdpEchoServer
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Set up CSMA helper
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Connect all nodes via CSMA
    NetDeviceContainer csmaDevices = csma.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(csmaDevices);

    // Create a UDP echo server application on node 1
    UdpEchoServerHelper echoServer(9); // port 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create a UDP echo client application on node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9); // send to node 1
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set up tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("udp-echo.tr");
    csma.EnableAsciiAll(stream);

    csma.EnablePcapAll("udp-echo", false);

    // Enable real-time scheduling
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}