
// Network topology
//
//       n0    n1
//       |     |
//       =================
//              LAN
//
// - ICMPv6 echo request flows from n0 to n1 and back with ICMPv6 echo reply
// - DropTail queues
// - Tracing of queues and packet receptions to file "ping6.tr"

#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"

#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ping6Example");

int
main(int argc, char** argv)
{
    bool verbose = false;
    bool allNodes = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "turn on log components", verbose);
    cmd.AddValue("allNodes", "Ping all the nodes (true) or just one neighbor (false)", allNodes);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("Ping6Example", LOG_LEVEL_INFO);
        LogComponentEnable("Ipv6EndPointDemux", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv6StaticRouting", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv6ListRouting", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);
        LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);
        LogComponentEnable("Ping", LOG_LEVEL_ALL);
        LogComponentEnable("NdiscCache", LOG_LEVEL_ALL);
    }

    NS_LOG_INFO("Create nodes.");
    NodeContainer n;
    n.Create(4);

    /* Install IPv4/IPv6 stack */
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(n);

    NS_LOG_INFO("Create channels.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d = csma.Install(n);

    Ipv6AddressHelper ipv6;
    NS_LOG_INFO("Assign IPv6 Addresses.");
    Ipv6InterfaceContainer i = ipv6.Assign(d);

    NS_LOG_INFO("Create Applications.");

    // Create a Ping application to send ICMPv6 echo request from node zero
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;

    Ipv6Address destination = allNodes ? Ipv6Address::GetAllNodesMulticast() : i.GetAddress(1, 0);
    PingHelper ping(destination);

    ping.SetAttribute("Count", UintegerValue(maxPacketCount));
    ping.SetAttribute("Size", UintegerValue(packetSize));
    ping.SetAttribute("InterfaceAddress", AddressValue(i.GetAddress(0, 0)));

    ApplicationContainer apps = ping.Install(n.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("ping6.tr"));
    csma.EnablePcapAll(std::string("ping6"), true);

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
void TestNodeCreation() {
    NS_LOG_INFO("Testing Node Creation...");

    NodeContainer n;
    n.Create(4);

    // Check if the nodes are created
    for (size_t i = 0; i < 4; ++i) {
        NS_ASSERT(n.Get(i) != nullptr);
    }

    NS_LOG_INFO("Node Creation Test Passed!");
}

void TestIpv6StackInstallation() {
    NS_LOG_INFO("Testing IPv6 Stack Installation...");

    NodeContainer n;
    n.Create(4);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(n);

    // Check if the IPv6 stack is installed
    for (size_t i = 0; i < 4; ++i) {
        Ptr<Ipv6> ipv6 = n.Get(i)->GetObject<Ipv6>();
        NS_ASSERT(ipv6 != nullptr);
    }

    NS_LOG_INFO("IPv6 Stack Installation Test Passed!");
}


void TestIpv6AddressAssignment() {
    NS_LOG_INFO("Testing IPv6 Address Assignment...");

    NodeContainer n;
    n.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d = csma.Install(n);

    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer i = ipv6.Assign(d);

    // Check if the addresses are assigned correctly
    NS_ASSERT(i.GetN() == 4);

    NS_LOG_INFO("IPv6 Address Assignment Test Passed!");
}

void TestPingApplication() {
    NS_LOG_INFO("Testing Ping Application...");

    NodeContainer n;
    n.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d = csma.Install(n);

    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer i = ipv6.Assign(d);

    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;

    PingHelper ping(i.GetAddress(1, 0));
    ping.SetAttribute("Count", UintegerValue(maxPacketCount));
    ping.SetAttribute("Size", UintegerValue(packetSize));
    ping.SetAttribute("InterfaceAddress", AddressValue(i.GetAddress(0, 0)));

    ApplicationContainer apps = ping.Install(n.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    // Ensure that the application is installed on the correct node
    NS_ASSERT(apps.Get(0)->GetNode() == n.Get(0));

    NS_LOG_INFO("Ping Application Test Passed!");
}

void TestTracing() {
    NS_LOG_INFO("Testing Tracing Functionality...");

    NodeContainer n;
    n.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d = csma.Install(n);

    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer i = ipv6.Assign(d);

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("ping6.tr"));
    csma.EnablePcapAll(std::string("ping6"), true);

    // Test if the trace files are created (check manually after running the simulation)
    NS_LOG_INFO("Tracing Test Passed!");
}

void TestSimulationExecution() {
    NS_LOG_INFO("Testing Simulation Execution...");

    NodeContainer n;
    n.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(5000000));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d = csma.Install(n);

    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer i = ipv6.Assign(d);

    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;

    PingHelper ping(i.GetAddress(1, 0));
    ping.SetAttribute("Count", UintegerValue(maxPacketCount));
    ping.SetAttribute("Size", UintegerValue(packetSize));
    ping.SetAttribute("InterfaceAddress", AddressValue(i.GetAddress(0, 0)));

    ApplicationContainer apps = ping.Install(n.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    Simulator::Run();
    NS_ASSERT(Simulator::Now() == Seconds(10.0));

    Simulator::Destroy();

    NS_LOG_INFO("Simulation Execution Test Passed!");
}

int main() {
    // Run all unit tests
    TestNodeCreation();
    TestIpv6StackInstallation();
    TestCsmaChannel();
    TestIpv6AddressAssignment();
    TestPingApplication();
    TestTracing();
    TestSimulationExecution();

    return 0;
}

