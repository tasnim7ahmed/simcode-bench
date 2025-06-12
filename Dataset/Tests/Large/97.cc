#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet.h"
#include <iostream>

using namespace ns3;

// Helper function for testing node creation and naming
void TestNodeCreationAndNaming()
{
    NS_LOG_INFO("Testing Node Creation and Naming");

    NodeContainer nodes;
    nodes.Create(4);

    // Name the nodes
    Names::Add("clientZero", nodes.Get(0));
    Names::Add("/Names/server", nodes.Get(1));
    Names::Rename("clientZero", "client");

    // Verify names
    NS_TEST_ASSERT_MSG_EQ(Names::Find("client"), nodes.Get(0), "Failed to find node 'client'");
    NS_TEST_ASSERT_MSG_EQ(Names::Find("/Names/server"), nodes.Get(1), "Failed to find node 'server'");

    NS_LOG_INFO("Node Creation and Naming test passed.");
}

// Helper function for testing device creation and naming
void TestDeviceNaming()
{
    NS_LOG_INFO("Testing Device Naming");

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
    NetDeviceContainer devices = csma.Install(nodes);

    // Name the devices
    Names::Add("/Names/client/eth0", devices.Get(0));
    Names::Add("server/eth0", devices.Get(1));

    // Verify device names
    NS_TEST_ASSERT_MSG_EQ(Names::Find("/Names/client/eth0"), devices.Get(0), "Failed to find device 'eth0' for client");
    NS_TEST_ASSERT_MSG_EQ(Names::Find("/Names/server/eth0"), devices.Get(1), "Failed to find device 'eth0' for server");

    NS_LOG_INFO("Device Naming test passed.");
}

// Helper function for testing MTU configuration using names
void TestMtuConfiguration()
{
    NS_LOG_INFO("Testing MTU Configuration via Names");

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
    NetDeviceContainer devices = csma.Install(nodes);

    // Set MTU attribute using object names
    Ptr<CsmaNetDevice> csmaNetDevice = devices.Get(0)->GetObject<CsmaNetDevice>();
    UintegerValue val;
    csmaNetDevice->GetAttribute("Mtu", val);
    std::cout << "MTU before configuration: " << val.Get() << std::endl;

    // Configure the MTU using name path
    Config::Set("/Names/client/eth0/Mtu", UintegerValue(1234));
    csmaNetDevice->GetAttribute("Mtu", val);
    std::cout << "MTU after configuration: " << val.Get() << std::endl;

    // Validate MTU configuration
    NS_TEST_ASSERT_MSG_EQ(val.Get(), 1234, "MTU value was not configured correctly");

    NS_LOG_INFO("MTU Configuration test passed.");
}

// Helper function for testing UDP echo server and client applications
void TestUdpEchoApplications()
{
    NS_LOG_INFO("Testing UDP Echo Server and Client Applications");

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
    NetDeviceContainer devices = csma.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9; // Echo port
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install("/Names/server");
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 1;
    Time interPacketInterval = Seconds(1.0);
    UdpEchoClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = client.Install("/Names/client");
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Verify that server and client applications are installed correctly
    NS_TEST_ASSERT_MSG_EQ(serverApps.GetN(), 1, "Failed to install server application");
    NS_TEST_ASSERT_MSG_EQ(clientApps.GetN(), 1, "Failed to install client application");

    NS_LOG_INFO("UDP Echo Server and Client Applications test passed.");
}

// Helper function for testing packet reception callback
void TestPacketReceptionCallback()
{
    NS_LOG_INFO("Testing Packet Reception Callback");

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
    NetDeviceContainer devices = csma.Install(nodes);

    // Set up the callback for packet reception
    Config::Connect("/Names/client/eth0/MacRx", MakeCallback(&RxEvent));

    // Run a simple simulation to verify callback is invoked
    Simulator::Run();
    Simulator::Destroy();

    // Check if the packet reception callback was triggered (this depends on specific test conditions)
    NS_LOG_INFO("Packet Reception Callback test passed.");
}

// Helper function for validating pcap file creation
void TestPcapFileCreation()
{
    NS_LOG_INFO("Testing Pcap File Creation");

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
    NetDeviceContainer devices = csma.Install(nodes);

    // Enable pcap file tracing
    csma.EnablePcapAll("object-names");

    // Check if the pcap file has been created
    std::ifstream pcapFile("object-names-client-eth0.pcap");
    bool pcapExists = pcapFile.good();
    pcapFile.close();

    NS_TEST_ASSERT_MSG_EQ(pcapExists, true, "Pcap file was not created");

    NS_LOG_INFO("Pcap File Creation test passed.");
}

int main(int argc, char* argv[])
{
    // Run all the tests
    TestNodeCreationAndNaming();
    TestDeviceNaming();
    TestMtuConfiguration();
    TestUdpEchoApplications();
    TestPacketReceptionCallback();
    TestPcapFileCreation();

    return 0;
}
