#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaNetworkTest");

void TestCsmaNodeCreation()
{
    NodeContainer csmaNodes;
    csmaNodes.Create(4); // Create 4 CSMA nodes
    NS_ASSERT_MSG(csmaNodes.GetN() == 4, "Expected 4 CSMA nodes.");
}

void TestCsmaChannelInstallation()
{
    NodeContainer csmaNodes;
    csmaNodes.Create(4); // Create 4 CSMA nodes

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);
    NS_ASSERT_MSG(csmaDevices.GetN() == 4, "Expected 4 CSMA devices to be installed.");
}

void TestInternetStackInstallation()
{
    NodeContainer csmaNodes;
    csmaNodes.Create(4); // Create 4 CSMA nodes

    InternetStackHelper stack;
    stack.Install(csmaNodes);

    Ptr<Node> node = csmaNodes.Get(0);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    NS_ASSERT_MSG(ipv4 != nullptr, "Expected Ipv4 stack to be installed on node.");
}

void TestIpAddressAssignment()
{
    NodeContainer csmaNodes;
    csmaNodes.Create(4); // Create 4 CSMA nodes

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    InternetStackHelper stack;
    stack.Install(csmaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    NS_ASSERT_MSG(csmaInterfaces.GetN() == 4, "Expected 4 IP addresses to be assigned.");
}

void TestTcpApplicationInstallation()
{
    NodeContainer csmaNodes;
    csmaNodes.Create(4); // Create 4 CSMA nodes

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    InternetStackHelper stack;
    stack.Install(csmaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    uint16_t port = 9; // TCP port

    // Create packet sink on node 0 (server)
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(csmaNodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create TCP clients on other nodes
    for (uint32_t i = 1; i < csmaNodes.GetN(); ++i)
    {
        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(csmaInterfaces.GetAddress(0), port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", StringValue("1Mbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = onoff.Install(csmaNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("TCP application test completed.");
}

void TestNetAnimGeneration()
{
    NodeContainer csmaNodes;
    csmaNodes.Create(4); // Create 4 CSMA nodes

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    InternetStackHelper stack;
    stack.Install(csmaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    uint16_t port = 9; // TCP port

    // Create a packet sink on node 0 (server)
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(csmaNodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Create TCP clients on other nodes
    for (uint32_t i = 1; i < csmaNodes.GetN(); ++i)
    {
        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(csmaInterfaces.GetAddress(0), port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", StringValue("1Mbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = onoff.Install(csmaNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable NetAnim
    AnimationInterface anim("csma_network.xml");
    anim.SetConstantPosition(csmaNodes.Get(0), 10.0, 20.0); // Server
    anim.SetConstantPosition(csmaNodes.Get(1), 20.0, 40.0); // Client 1
    anim.SetConstantPosition(csmaNodes.Get(2), 30.0, 40.0); // Client 2
    anim.SetConstantPosition(csmaNodes.Get(3), 40.0, 40.0); // Client 3

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    // Check if the animation file is created
    std::ifstream file("csma_network.xml");
    NS_ASSERT_MSG(file.good(), "NetAnim XML file was not generated.");
}

int main(int argc, char *argv[])
{
    TestCsmaNodeCreation();
    TestCsmaChannelInstallation();
    TestInternetStackInstallation();
    TestIpAddressAssignment();
    TestTcpApplicationInstallation();
    TestNetAnimGeneration();

    return 0;
}
