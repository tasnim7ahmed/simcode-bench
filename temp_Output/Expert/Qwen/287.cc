#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/mmwave-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MmWave5GSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer gnbNodes;
    NodeContainer ueNodes;
    gnbNodes.Create(1);
    ueNodes.Create(2);

    MobilityHelper mobilityGnb;
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                                "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=1.5]"));
    mobilityUe.Install(ueNodes);

    MmWaveHelper mmWave = MmWaveHelper::Default();
    mmWave.SetSchedulerType("ns3::MmWaveFlexTtiMacScheduler");
    mmWave.SetPhyAttribute("NumRbPerRbg", UintegerValue(1));
    mmWave.SetSpectrumChannelType("ns3::MmWaveSpectrumChannel");
    mmWave.Initialize();

    NetDeviceContainer gnbDevs = mmWave.InstallGnbDevice(gnbNodes, ueNodes);
    NetDeviceContainer ueDevs = mmWave.InstallUeDevice(ueNodes, gnbDevs);

    InternetStackHelper internet;
    internet.Install(NodeContainer(gnbNodes, ueNodes));

    Ipv4AddressHelper ipv4; 
    ipv4.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueInterfaces = ipv4.Assign(ueDevs);
    Ipv4InterfaceContainer gnbInterface = ipv4.Assign(gnbDevs);

    uint16_t port = 5001;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(ueInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(10.0));

    mmWave.EnableTraces();

    AsciiTraceHelper ascii;
    mmWave.EnableAsciiAll(ascii.CreateFileStream("mmwave-trace.tr"));

    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        mmWave.EnablePcap("ue" + std::to_string(i), ueDevs.Get(i), true, true);
    }
    for (uint32_t i = 0; i < gnbDevs.GetN(); ++i)
    {
        mmWave.EnablePcap("gnb" + std::to_string(i), gnbDevs.Get(i), true, true);
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}