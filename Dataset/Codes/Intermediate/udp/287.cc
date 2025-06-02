#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mmwave-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrMmWaveExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("NrMmWaveExample", LOG_LEVEL_INFO);

    // Create gNB (base station) and UE nodes
    NodeContainer ueNodes, gnbNode;
    ueNodes.Create(2);
    gnbNode.Create(1);

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNode);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(ueNodes);

    // Install NR mmWave module
    Ptr<MmWaveHelper> mmWaveHelper = CreateObject<MmWaveHelper>();
    NetDeviceContainer gnbDevice = mmWaveHelper->InstallGnbDevice(gnbNode);
    NetDeviceContainer ueDevices = mmWaveHelper->InstallUeDevice(ueNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueInterfaces = mmWaveHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevices));

    // Attach UE nodes to gNB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        mmWaveHelper->Attach(ueDevices.Get(i), gnbDevice.Get(0));
    }

    // Set up UDP traffic
    uint16_t port = 5001;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper udpClient(ueInterfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    mmWaveHelper->EnablePcapAll("nr-mmwave");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
