import adsk.core, adsk.fusion, traceback, math


def _helix_nurbs(R, lead, z0, z1, deg_per_seg=10.0):
    """构造真三维螺旋线 NurbsCurve3D（立方贝塞尔分段合并）。单位 cm。"""
    total_angle = 2 * math.pi * (z1 - z0) / lead
    n_seg = max(1, int(math.ceil(total_angle / math.radians(deg_per_seg))))
    dth = total_angle / n_seg

    def pt(th):
        return adsk.core.Point3D.create(
            R * math.cos(th), R * math.sin(th), z0 + lead * th / (2 * math.pi))

    def tan(th):
        return (-R * math.sin(th), R * math.cos(th), lead / (2 * math.pi))

    curve = None
    for i in range(n_seg):
        a = i * dth
        b = (i + 1) * dth
        p0 = pt(a)
        p3 = pt(b)
        ta = tan(a)
        tb = tan(b)
        p1 = adsk.core.Point3D.create(
            p0.x + (dth / 3.0) * ta[0], p0.y + (dth / 3.0) * ta[1], p0.z + (dth / 3.0) * ta[2])
        p2 = adsk.core.Point3D.create(
            p3.x - (dth / 3.0) * tb[0], p3.y - (dth / 3.0) * tb[1], p3.z - (dth / 3.0) * tb[2])
        bez = adsk.core.NurbsCurve3D.createNonRational(
            [p0, p1, p2, p3], 3,
            [0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0], False)
        curve = bez if curve is None else curve.merge(bez)
    return curve


def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        doc = app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
        product = doc.products.item(0)
        design = adsk.fusion.Design.cast(product)
        root = design.rootComponent

        # ---- 螺母配套丝杆（布尔刀具）参数（毫米）----
        PITCH = 5.5
        STARTS = 4
        LEAD = PITCH * STARTS      # 导程 22 mm
        OD = 11.85                # 刀具大径 = 螺母内螺纹大径
        ID = 6.65                 # 刀具小径 = 螺母内螺纹小径
        LENGTH = 30.0             # 螺纹有效长度
        FLANK_DEG = 15.0          # 单侧牙型角（30° 梯形 → 半角 15°）
        # 牙型轴向半宽：牙底取 P/4，牙顶按 30° 牙型角自动推导
        ROOT_HALF = PITCH / 4.0
        CREST_HALF = ROOT_HALF - ((OD - ID) / 2.0) * math.tan(math.radians(FLANK_DEG))
        # ---------------------------------------------------------

        cm = 0.1
        sketches = root.sketches
        feats = root.features

        # 1. 小径芯轴圆柱（沿 +Z，长度 = LENGTH）
        sk0 = sketches.add(root.xYConstructionPlane)
        sk0.sketchCurves.sketchCircles.addByCenterRadius(
            adsk.core.Point3D.create(0, 0, 0), ID / 2 * cm)
        ei = feats.extrudeFeatures.createInput(
            sk0.profiles.item(0),
            adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
        ei.setOneSideExtent(
            adsk.fusion.DistanceExtentDefinition.create(
                adsk.core.ValueInput.createByReal(LENGTH * cm)),
            adsk.fusion.ExtentDirections.PositiveExtentDirection)
        base = feats.extrudeFeatures.add(ei)

        # 2. 30° 梯形牙型截面（XZ 平面：x = 径向, y = 轴向）
        ri, ro = ID / 2 * cm, OD / 2 * cm
        hb, hc = ROOT_HALF * cm, CREST_HALF * cm
        sk1 = sketches.add(root.xZConstructionPlane)
        ln = sk1.sketchCurves.sketchLines
        pa = adsk.core.Point3D.create(ri, -hb, 0)  # 牙底-后（宽）
        pb = adsk.core.Point3D.create(ri,  hb, 0)  # 牙底-前（宽）
        pc = adsk.core.Point3D.create(ro,  hc, 0)  # 牙顶-前（窄）
        pd = adsk.core.Point3D.create(ro, -hc, 0)  # 牙顶-后（窄）
        for a, b in ((pa, pb), (pb, pc), (pc, pd), (pd, pa)):
            ln.addByTwoPoints(a, b)
        tooth = sk1.profiles.item(0)

        # 3a. 内螺旋路径（小径面）
        inner = _helix_nurbs(ri, LEAD * cm, 0.0, LENGTH * cm)
        sk2 = sketches.add(root.xYConstructionPlane)
        inner_cv = sk2.sketchCurves.sketchFixedSplines.addByNurbsCurve(inner)
        path = adsk.fusion.Path.create(
            inner_cv, adsk.fusion.ChainedCurveOptions.noChainedCurves)

        # 3b. 外螺旋引导线（大径面，锁截面方向防扭转）
        outer = _helix_nurbs(ro, LEAD * cm, 0.0, LENGTH * cm)
        sk3 = sketches.add(root.xYConstructionPlane)
        outer_cv = sk3.sketchCurves.sketchFixedSplines.addByNurbsCurve(outer)
        guide = adsk.fusion.Path.create(
            outer_cv, adsk.fusion.ChainedCurveOptions.noChainedCurves)

        # 4. 沿内螺旋扫掠，外螺旋做引导线 → 牙高全程均匀
        si = feats.sweepFeatures.createInput(
            tooth, path,
            adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
        si.guideRail = guide
        si.profileScaling = adsk.fusion.SweepProfileScalingOptions.SweepProfileNoScalingOption
        sweep = feats.sweepFeatures.add(si)

        # 5. 绕 Z 轴圆周阵列 4 头
        bodies = adsk.core.ObjectCollection.create()
        bodies.add(sweep.bodies.item(0))
        ci = feats.circularPatternFeatures.createInput(bodies, root.zConstructionAxis)
        ci.quantity = adsk.core.ValueInput.createByReal(STARTS)
        ci.totalAngle = adsk.core.ValueInput.createByString('360 deg')
        cp = feats.circularPatternFeatures.add(ci)

        # 6. 合并芯轴与全部螺纹牙，成为单一刀具实体
        tools = adsk.core.ObjectCollection.create()
        tools.add(sweep.bodies.item(0))
        for b in cp.bodies:
            tools.add(b)
        cbi = feats.combineFeatures.createInput(base.bodies.item(0), tools)
        cbi.operation = adsk.fusion.FeatureOperations.JoinFeatureOperation
        feats.combineFeatures.add(cbi)

        info = '螺母配套丝杆（布尔刀具）生成完成\n大径 %.2f / 小径 %.2f / 长 %.1f / 4头 / 30°' % (OD, ID, LENGTH)
        ui.messageBox(info)

    except:
        if ui:
            ui.messageBox('运行异常:\n{}'.format(traceback.format_exc()))
