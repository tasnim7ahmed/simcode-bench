#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteBasicSimulation");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("LteBasicSimulation", LOG_LEVEL_INFO);

    // Create LTE eNodeB and UE nodes
    NodeContainer ueNodes, enbNode;
    ueNodes.Create(3);
    enbNode.Create(1);

    // Set up mobility models
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNode);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
    mobility.Install(ueNodes);

    // Install LTE module
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbDevice = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueDevices = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueInterfaces = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevices));

    // Attach UE nodes to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueDevices.Get(i), enbDevice.Get(0));
    }

    // Set up a UDP server on UE 0
    uint16_t port = 5000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up a UDP client on UE 1 sending to UE 0
    UdpClientHelper udpClient(ueInterfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    lteHelper->EnablePcapAll("lte-basic");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
