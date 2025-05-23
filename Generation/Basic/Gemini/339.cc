#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main(int argc, char* argv[])
{
    // 1. Set up simulation parameters
    uint16_t port = 9;                       // TCP port for communication
    std::string linkDataRate = "5Mbps";      // Point-to-point link data rate
    std::string linkDelay = "2ms";           // Point-to-point link delay
    uint32_t packetSize = 512;               // Size of TCP packets in bytes
    double simulationTime = 10.0;            // Total simulation time in seconds

    // Calculate OnTime and OffTime for OnOffApplication to send 100 packets (512 bytes each)
    // within a 0.1-second cycle, utilizing the specified link data rate.
    // Total data per burst = 100 packets * 512 bytes/packet = 51200 bytes.
    // Convert to bits: 51200 bytes * 8 bits/byte = 409600 bits.
    // Time to send this data at linkDataRate (5Mbps = 5,000,000 bps):
    // OnTime = 409600 bits / 5,000,000 bps = 0.08192 seconds.
    // Given the requirement "every 0.1 seconds", this is the cycle time.
    // OffTime = CycleTime - OnTime = 0.1 seconds - 0.08192 seconds = 0.01808 seconds.
    double onTime = 0.08192;
    double offTime = 0.01808;

    // Start client slightly after server to ensure server is ready
    double clientStart = 1.0; 

    // 2. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 3. Create point-to-point link and install devices
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue(linkDataRate));
    p2p.SetChannelAttribute("Delay", ns3::StringValue(linkDelay));
    ns3::NetDeviceContainer devices = p2p.Install(nodes);

    // 4. Install Internet stack (TCP/IP) on nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. Set up PacketSink (Server) application
    // Server is Node 1 (nodes.Get(1))
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", 
                                     ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(1));
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(simulationTime));

    // 7. Set up OnOff (Client) application
    // Client is Node 0 (nodes.Get(0))
    ns3::OnOffHelper clientHelper("ns3::TcpSocketFactory", ns3::Ipv4Address::GetZero()); // Temporary address, will be set below
    clientHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(onTime) + "]"));
    clientHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(offTime) + "]"));
    clientHelper.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    clientHelper.SetAttribute("DataRate", ns3::StringValue(linkDataRate)); // Client sends at link's max rate during ON period

    // Set the remote address (server's IP and port)
    ns3::Address serverAddress = ns3::InetSocketAddress(interfaces.GetAddress(1), port);
    clientHelper.SetAttribute("Remote", ns3::AddressValue(serverAddress));

    ns3::ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(clientStart));
    clientApps.Stop(ns3::Seconds(simulationTime));

    // 8. Run simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}