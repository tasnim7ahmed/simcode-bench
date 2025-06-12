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
#include "ns3/ipv4-interface-container.h"
#include "ns3/application-container.h"
#include "ns3/net-device-container.h"
#include "ns3/data-rate.h"
#include "ns3/simulator.h"
#include "ns3/ascii-trace-helper.h"
#include "ns3/ptr.h"
#include "ns3/node-container.h"
#include "ns3/address.h"
#include <vector>
#include <sstream>
#include <iostream>

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
    NetDeviceContainer backboneDevices;
    Ipv4InterfaceContainer backboneInterfaces;
    std::vector<NodeContainer> staNodes;
    std::vector<NetDeviceContainer> staDevices;
    std::vector<NetDeviceContainer> apDevices;
    std::vector<Ipv4InterfaceContainer> staInterfaces;
    std::vector<Ipv4InterfaceContainer> apInterfaces;

    InternetStackHelper stack;
    CsmaHelper csma;
    Ipv4AddressHelper ip;
    ip.SetBase("192.168.0.0", "255.255.255.0");

    backboneNodes.Create(nWifis);
    stack.Install(backboneNodes);

    backboneDevices = csma.Install(backboneNodes);

    double wifiX = 0.0;

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    for (uint32_t i = 0; i < nWifis; ++i)
    {
        // calculate ssid for wifi subnetwork
        std::ostringstream oss;
        oss << "wifi-default-" << i;
        Ssid ssid = Ssid(oss.str());

        NodeContainer sta;
        NetDeviceContainer staDev;
        NetDeviceContainer apDev;
        Ipv4InterfaceContainer staInterface;
        Ipv4InterfaceContainer apInterface;

        MobilityHelper mobility;
        BridgeHelper bridge;
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211g);
        WifiMacHelper wifiMac;
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        Ptr<YansWifiChannel> chan = wifiChannel.Create();
        wifiPhy.SetChannel(chan);

        sta.Create(nStas);

        // Set STA positions
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(wifiX),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Mode", StringValue("Time"),
                                  "Time", StringValue("2s"),
                                  "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                                  "Bounds", RectangleValue(Rectangle(wifiX, wifiX + 5.0, 0.0, (nStas + 1) * 5.0)));
        mobility.Install(sta);

        stack.Install(sta);

        // Setup the AP
        MobilityHelper mobilityAp;
        mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobilityAp.Install(backboneNodes.Get(i));

        wifiMac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(ssid));
        apDev = wifi.Install(wifiPhy, wifiMac, backboneNodes.Get(i));

        NetDeviceContainer bridgeDev;
        bridgeDev = bridge.Install(backboneNodes.Get(i), NetDeviceContainer(apDev, backboneDevices.Get(i)));

        // assign AP IP address to bridge, not wifi
        apInterface = ip.Assign(bridgeDev);

        // Setup the STAs
        wifiMac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "ActiveProbing", BooleanValue(false));
        staDev = wifi.Install(wifiPhy, wifiMac, sta);
        staInterface = ip.Assign(staDev);

        // save everything in containers.
        staNodes.push_back(sta);
        apDevices.push_back(apDev);
        apInterfaces.push_back(apInterface);
        staDevices.push_back(staDev);
        staInterfaces.push_back(staInterface);

        wifiX += 20.0;
    }

    // Application Setup
    Address dest;
    std::string protocol;
    if (sendIp && nWifis > 1 && nStas > 1)
    {
        dest = InetSocketAddress(staInterfaces[1].GetAddress(1), 1025);
        protocol = "ns3::UdpSocketFactory";
    }
    else if (!sendIp && nWifis > 1 && nStas > 1)
    {
        PacketSocketAddress tmp;
        tmp.SetSingleDevice(staDevices[0].Get(0)->GetIfIndex());
        tmp.SetPhysicalAddress(staDevices[1].Get(0)->GetAddress());
        tmp.SetProtocol(0x807);
        dest = tmp;
        protocol = "ns3::PacketSocketFactory";
    }
    else
    {
        // Not enough stations/wifis to meaningfully run the application
        std::cerr << "Simulation requires at least 2 wifi networks with at least 2 stations each for application traffic.\n";
        Simulator::Stop(Seconds(0.1));
        Simulator::Run();
        Simulator::Destroy();
        return 1;
    }

    OnOffHelper onoff(protocol, dest);
    onoff.SetConstantRate(DataRate("500kb/s"));
    ApplicationContainer apps = onoff.Install(staNodes[0].Get(0));
    apps.Start(Seconds(0.5));
    apps.Stop(Seconds(3.0));

    wifiPhy.EnablePcap("wifi-wired-bridging-ap0", apDevices[0]);
    if (apDevices.size() > 1)
      wifiPhy.EnablePcap("wifi-wired-bridging-ap1", apDevices[1]);

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