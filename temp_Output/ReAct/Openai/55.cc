#include "ns3/bridge-helper.h"
#include "ns3/command-line.h"
#include "ns3/csma-helper.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/rectangle.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/ascii-trace-helper.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    uint32_t nWifis = 2;
    uint32_t nStas = 2;
    bool sendIp = true;
    bool writeMobility = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifis", "Number of wifi networks", nWifis);
    cmd.AddValue("nStas", "Number of stations per wifi network", nStas);
    cmd.AddValue("SendIp", "Send Ipv4 or raw packets", sendIp);
    cmd.AddValue("writeMobility", "Write mobility trace", writeMobility);
    cmd.Parse(argc, argv);

    NodeContainer backboneNodes;
    backboneNodes.Create(nWifis);

    std::vector<NodeContainer> staNodes;
    std::vector<NetDeviceContainer> staDevices;
    std::vector<NetDeviceContainer> apDevices;
    std::vector<Ipv4InterfaceContainer> staInterfaces;
    std::vector<Ipv4InterfaceContainer> apInterfaces;

    InternetStackHelper stack;
    stack.Install(backboneNodes);

    CsmaHelper csma;
    NetDeviceContainer backboneDevices = csma.Install(backboneNodes);

    Ipv4AddressHelper ip;
    ip.SetBase("192.168.0.0", "255.255.255.0");

    double wifiX = 0.0;

    for (uint32_t i = 0; i < nWifis; ++i)
    {
        std::ostringstream oss;
        oss << "wifi-default-" << i;
        Ssid ssid = Ssid(oss.str());

        NodeContainer sta;
        sta.Create(nStas);
        stack.Install(sta);

        // Mobility for STA and AP
        MobilityHelper mobilitySta;
        mobilitySta.SetPositionAllocator("ns3::GridPositionAllocator",
                                         "MinX", DoubleValue(wifiX),
                                         "MinY", DoubleValue(0.0),
                                         "DeltaX", DoubleValue(5.0),
                                         "DeltaY", DoubleValue(5.0),
                                         "GridWidth", UintegerValue(1),
                                         "LayoutType", StringValue("RowFirst"));
        mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                     "Mode", StringValue("Time"),
                                     "Time", StringValue("2s"),
                                     "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                                     "Bounds", RectangleValue(Rectangle(wifiX, wifiX + 5.0, 0.0, (nStas + 1) * 5.0)));
        mobilitySta.Install(sta);

        MobilityHelper mobilityAp;
        mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobilityAp.Install(backboneNodes.Get(i));

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());
        wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

        WifiHelper wifi;
        wifi.SetRemoteStationManager("ns3::AarfWifiManager");

        WifiMacHelper wifiMac;

        NetDeviceContainer staDev;
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        staDev = wifi.Install(wifiPhy, wifiMac, sta);

        NetDeviceContainer apDev;
        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        apDev = wifi.Install(wifiPhy, wifiMac, backboneNodes.Get(i));

        BridgeHelper bridge;
        NetDeviceContainer bridgeDev = bridge.Install(backboneNodes.Get(i), NetDeviceContainer(apDev, backboneDevices.Get(i)));

        Ipv4InterfaceContainer apInterface = ip.Assign(bridgeDev);
        Ipv4InterfaceContainer staInterface = ip.Assign(staDev);

        staNodes.push_back(sta);
        staDevices.push_back(staDev);
        apDevices.push_back(apDev);
        staInterfaces.push_back(staInterface);
        apInterfaces.push_back(apInterface);

        wifiX += 20.0;
    }

    Address dest;
    std::string protocol;
    if (sendIp)
    {
        dest = InetSocketAddress(staInterfaces[1].GetAddress(1), 1025);
        protocol = "ns3::UdpSocketFactory";
    }
    else
    {
        PacketSocketAddress tmp;
        tmp.SetSingleDevice(staDevices[0].Get(0)->GetIfIndex());
        tmp.SetPhysicalAddress(staDevices[1].Get(0)->GetAddress());
        tmp.SetProtocol(0x807);
        dest = tmp;
        protocol = "ns3::PacketSocketFactory";
    }

    OnOffHelper onoff(protocol, dest);
    onoff.SetConstantRate(DataRate("500kb/s"));
    ApplicationContainer apps = onoff.Install(staNodes[0].Get(0));
    apps.Start(Seconds(0.5));
    apps.Stop(Seconds(3.0));

    YansWifiPhyHelper wifiPhyPcap = YansWifiPhyHelper::Default();
    wifiPhyPcap.EnablePcap("wifi-wired-bridging", apDevices[0]);
    if (apDevices.size() >= 2)
        wifiPhyPcap.EnablePcap("wifi-wired-bridging", apDevices[1]);

    if (writeMobility)
    {
        AsciiTraceHelper ascii;
        MobilityHelper::EnableAsciiAll(ascii.CreateFileStream("wifi-wired-bridging.mob"));
    }

    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}