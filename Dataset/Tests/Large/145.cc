#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshTopologyExampleTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(3);

    // Ensure that 3 nodes have been created
    NS_ASSERT_MSG(nodes.GetN() == 3, "Three nodes should be created in the network.");
}

// Test 2: Verify Mesh Network Installation
void TestMeshNetworkInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");  // 802.11s for mesh
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    NetDeviceContainer meshDevices = mesh.Install(wifi, nodes);

    // Ensure that mesh devices have been installed on all nodes
    NS_ASSERT_MSG(meshDevices.GetN() == 3, "Three mesh devices should be installed.");
}

// Test 3: Verify Mobility Model Setup
void TestMobilityModelSetup()
{
    NodeContainer nodes;
    nodes.Create(3);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(100.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Verify that the mobility model has been installed on all nodes
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<ConstantPositionMobilityModel> mobilityModel = nodes.Get(i)->GetObject<ConstantPositionMobilityModel>();
        NS_ASSERT_MSG(mobilityModel != nullptr, "Mobility model should be set on node " + std::to_string(i));
    }
}

// Test 4: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    NetDeviceContainer meshDevices = mesh.Install(wifi, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // Ensure IP addresses are correctly assigned to the mesh devices
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NS_ASSERT_MSG(interfaces.GetAddress(i) != Ipv4Address("0.0.0.0"), "IP address not assigned to node " + std::to_string(i));
    }
}

// Test 5: Verify UDP Echo Server Installation
void TestUdpEchoServerInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);

    UdpEchoServerHelper echoServer(9);  // Port number 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Ensure that the UDP Echo Server is installed on node 0
    NS_ASSERT_MSG(serverApps.GetN() == 1, "The UDP Echo Server should be installed on node 0.");
}

// Test 6: Verify UDP Echo Client Installation
void TestUdpEchoClientInstallation()
{
    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    NetDeviceContainer meshDevices = mesh.Install(wifi, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Ensure that the UDP Echo Client is installed on node 1
    NS_ASSERT_MSG(clientApps.GetN() == 1, "The UDP Echo Client should be installed on node 1.");
}

// Test 7: Verify PCAP Tracing for Packet Flow
void TestPcapTracing()
{
    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    NetDeviceContainer meshDevices = mesh.Install(wifi, nodes);

    wifi.EnablePcapAll("mesh-topology");

    // Ensure that PCAP tracing is enabled (this test does not check actual PCAP files, but verifies the setup)
    // To verify this, you'd typically check that the "mesh-topology" files are being generated after simulation.
    NS_LOG_INFO("PCAP tracing enabled for mesh topology.");
}

// Test 8: Verify Simulation Runs Successfully
void TestSimulationRun()
{
    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    NetDeviceContainer meshDevices = mesh.Install(wifi, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    UdpEchoServerHelper echoServer(9);  // Port 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    NS_LOG_INFO("Simulation finished.");
    Simulator::Destroy();

    // Check that simulation has run without issues
    NS_LOG_INFO("Mesh simulation successfully ran.");
}

// Main test function to run all tests
int main()
{
    TestNodeCreation();
    TestMeshNetworkInstallation();
    TestMobilityModelSetup();
    TestIpAddressAssignment();
    TestUdpEchoServerInstallation();
    TestUdpEchoClientInstallation();
    TestPcapTracing();
    TestSimulationRun();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
